local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')

local function obs_connector_lowlevel()
    local instance = {}

    instance.promises = deck:PromiseList()

    instance.handshaking = false

    instance.socket = deck.connector_factory.Websocket()
    instance.socket.port = 4455
    instance.socket.protocol = 'obswebsocket.json'

    instance.socket.on_connect_failed = function(sock, err)
        if instance.on_connect_failed then
            logger(logger.DEBUG, 'Connect to OBS instance failed: ' .. err)
            instance:on_connect_failed(err)
        else
            logger(logger.WARNING, 'Connect to OBS instance failed: ' .. err)
        end
    end

    instance.socket.on_connect = function(sock, protocol)
        if protocol ~= 'obswebsocket.json' then
            instance.socket.enabled = false
            if instance.on_connect_failed then
                logger(logger.DEBUG, 'Connect to OBS instance failed: remote protocol invalid')
                instance:on_connect_failed('Remote protocol invalid')
            else
                logger(logger.ERROR, 'Connect to OBS instance failed: remote protocol invalid')
            end
        else
            logger(logger.DEBUG, 'Connected to OBS instance, starting handshake ...')
            instance.handshaking = deck.clock
        end
    end

    instance.socket.on_disconnect = function(sock, msg)
        instance.handshaking = false
        if instance.on_disconnect then
            logger(logger.DEBUG, 'Connection to OBS instance closed: ' .. msg)
            instance:on_disconnect(msg)
        else
            logger(logger.INFO, 'Connection to OBS instance closed: ' .. msg)
        end
    end

    instance.socket.on_message = function(sock, msg)
        local data = util.from_json(msg)
        --print(util.to_json(data, true))

        if data.op == 0 then
            local reply = {}
            reply.op = 1
            reply.d = {}
            reply.d.rpcVersion = 1
            reply.d.eventSubscriptions = 2 ^ 11 - 1 --all low-volume events

            if data.d.authentication then
                local base64_secret = util.to_base64(util.sha256(instance.password .. data.d.authentication.salt))
                local authentication = util.to_base64(util.sha256(base64_secret .. data.d.authentication.challenge))
                reply.d.authentication = authentication
            end

            sock:write(util.to_json(reply))
        end

        if data.op == 2 then
            instance.handshaking = false
            if instance.on_connect then
                logger(logger.DEBUG, 'Connected to OBS!')
                instance:on_connect()
            else
                logger(logger.INFO, 'Connected to OBS!')
            end
        end

        if data.op == 5 then
            if instance.on_event then
                instance:on_event(data.d.eventType, data.d.eventData or {})
            end
        end

        if data.op == 7 then
            local result = {}
            result.ok = data.d.requestStatus.result
            result.status = data.d.requestStatus
            result.data = data.d.responseData or {}
            instance.promises:fulfill(data.d.requestId, result)

            if instance.on_response then
                instance:on_response(
                    data.d.requestType,
                    result)
            end
        end
    end

    instance.tick_inputs = function(self, clock)
        self.socket:tick_inputs(clock)

        if instance.handshaking and clock > instance.handshaking + 2000 then
            local err = 'Did not receive handshake from OBS within time limit'
            logger(logger.ERROR, err)

            instance.handshaking = false
            instance.socket.enabled = false
            if instance.on_connect_failed then
                instance:on_connect_failed(err)
            end
        end
    end

    instance.tick_outputs = function(self)
        self.socket:tick_outputs()
    end

    instance.shutdown = function(self)
        self.socket:shutdown()
    end

    instance.send_request = function(self, typ, data)
        local request_id = util.to_hex(util.random_bytes(8))
        local request = {}
        request.op = 6
        request.d = {}
        request.d.requestType = typ
        request.d.requestId = request_id
        if data then
            request.d.requestData = data
        end
        local promise = self.promises:new(request_id)
        instance.socket:write(util.to_json(request))
        return promise
    end

    return instance
end

deck.connector_factory.OBS_LowLevel = obs_connector_lowlevel

local function obs_connector()
    local instance = deck.connector_factory.OBS_LowLevel()

    local function set_value(key, value)
        if instance[key] ~= value then
            instance[key] = value

            local func_name = instance[key .. '_event']
            if func_name then
                local func = instance[func_name]
                if func then
                    local ok, err = pcall(func, instance, value)
                    if not ok then
                        logger(logger.ERROR, 'Error during callback', func_name, ':', err)
                    end
                end
            end
        end
    end

    local function make_value(key, value, event)
        instance[key] = value
        instance[key .. '_event'] = event
        instance[event] = function(self, data)
            logger(logger.DEBUG, 'OBS.' .. event .. ': ' .. util.to_json(data, true))
        end
    end

    instance.InputKinds = {}

    make_value('StreamActive', nil, 'on_stream_active_changed')
    make_value('StreamStatus', {}, 'on_stream_status_changed')
    make_value('RecordActive', nil, 'on_record_active_changed')
    make_value('RecordStatus', {}, 'on_record_status_changed')
    make_value('Scenes', {}, 'on_scenes_changed')
    make_value('Scene', '', 'on_scene_changed')
    make_value('SceneItems', '', 'on_scene_items_changed')
    make_value('Inputs', {}, 'on_inputs_changed')
    make_value('AudioInputs', {}, 'on_audio_inputs_changed')

    instance.on_connect = function(self)
        self:send_request('GetStreamStatus')
        self:send_request('GetRecordStatus')
        self:send_request('GetInputList')
        self:send_request('GetSpecialInputs')
        self:send_request('GetSceneList')
    end

    instance.on_event = function(self, typ, data)
        if typ == 'StreamStateChanged' then
            set_value('StreamActive', data.outputActive)
            self:send_request('GetStreamStatus')
            return
        end

        if typ == 'RecordStateChanged' then
            set_value('RecordActive', data.outputActive)
            self:send_request('GetRecordStatus')
            return
        end

        if typ == 'SceneCreated' or typ == 'SceneRemoved' or typ == 'SceneNameChanged' or typ == 'SceneListChanged' then
            self:send_request('GetSceneList')
            return
        end

        if typ == 'CurrentProgramSceneChanged' then
            set_value('Scene', data.sceneName)
            self:send_request('GetSceneItemList', { sceneName = data.sceneName })
            return
        end
    end

    instance.on_response = function(self, typ, response)
        if not response.ok then
            logger(logger.WARNING, 'OBS request ' .. typ .. ' failed with code ' .. tostring(response.status.code))
            return
        end

        if typ == 'GetStreamStatus' then
            set_value('StreamStatus', response.data)
            if self.StreamActive == nil then
                set_value('StreamActive', response.data.outputActive)
            end
            return
        end

        if typ == 'GetRecordStatus' then
            set_value('RecordStatus', response.data)
            if self.RecordActive == nil then
                set_value('RecordActive', response.data.outputActive)
            end
            return
        end

        if typ == 'GetSceneList' then
            local scenes = {}
            for _, v in ipairs(response.data.scenes) do
                scenes[v.sceneName] = v
            end
            set_value('Scenes', scenes)
            set_value('Scene', response.data.currentProgramSceneName)
            self:send_request('GetSceneItemList', { sceneName = response.data.currentProgramSceneName })
            return
        end

        if typ == 'GetSceneItemList' then
            local items = {}
            for _, v in ipairs(response.data.sceneItems) do
                items[v.sourceName] = v
            end
            set_value('SceneItems', items)
            return
        end

        if typ == 'GetInputList' then
            local inputs = {}
            for _, v in ipairs(response.data.inputs) do
                inputs[v.inputName] = v
            end
            set_value('Inputs', inputs)
            return
        end

        if typ == 'GetSpecialInputs' then
            set_value('AudioInputs', response.data)
            return
        end
    end

    -- User functions
    instance.ToggleStream = function(self)
        return self:send_request('ToggleStream')
    end

    instance.StartStream = function(self)
        return self:send_request('StartStream')
    end

    instance.StopStream = function(self)
        return self:send_request('StopStream')
    end

    instance.SetScene = function(self, name)
        return self:send_request('SetCurrentProgramScene', { sceneName = name })
    end

    instance.GetStats = function(self)
        local promise = self:send_request('GetStats')
        local result = promise:wait()
        if result.ok then
            return result.data
        end
    end

    instance.GetInputSettings = function(self, input_name)
        local input = self.Inputs[input_name]
        if input then
            local kind = self.InputKinds[input.inputKind]
            if not kind then
                local promise = self:send_request('GetInputDefaultSettings', { inputKind = input.inputKind })
                local result = promise:wait()
                if result.ok then
                    kind = result.data.defaultInputSettings
                    self.InputKinds[input.inputKind] = kind
                end
            end
            local promise = self:send_request('GetInputSettings', { inputUuid = input.inputUuid })
            local result = promise:wait()
            if result.ok then
                result.data.defaultInputSettings = kind
                return result.data
            end
        end
    end

    instance.SetInputSettings = function(self, input_name, settings, overlay)
        local request_data = {}
        request_data.inputSettings = settings
        request_data.overlay = overlay

        local input = self.Inputs[input_name]
        if input then
            request_data.inputUuid = input.inputUuid
        else
            request_data.inputName = input_name
        end

        return self:send_request('SetInputSettings', request_data)
    end

    return instance
end

deck.connector_factory.OBS = obs_connector


local function oauth2_connector()
    local instance = {}

    instance.ports = { 3000, 3001, 3002, 3003 }
    instance.server = deck.connector_factory.ServerSocket()
    instance.promise = deck:Promise(60000)

    instance.server.on_connect_failed = function(server, err)
        if instance.port_index < #instance.ports then
            instance.port_index = instance.port_index + 1
            server.port = instance.ports[instance.port_index]
            server:reset_timer()
        else
            instance.promise:fulfill(false)

            local msg = 'OAuth2 failed to bind to any port from list ' .. util.to_json(instance.ports)
            server.enabled = false
            if instance.on_connect_failed then
                logger(logger.DEBUG, msg)
                instance:on_connect_failed(err)
            else
                logger(logger.WARNING, msg)
            end
        end
    end

    instance.server.on_connect = function(server)
        local message = 'OAuth2 bound to port ' .. server.port .. ' successfully'
        if instance.on_connect then
            logger(logger.DEBUG, message)
            instance:on_connect(server.port)
        else
            logger(logger.INFO, message)
        end
    end

    instance.server.on_disconnect = function(server, msg)
        instance.promise:fulfill(false)

        local message = 'OAuth2 on port ' .. server.port .. ' stopped: ' .. msg
        if instance.on_disconnect then
            logger(logger.DEBUG, message)
            instance:on_disconnect(message)
        else
            logger(logger.INFO, message)
        end
    end

    instance.server.on_accept = function(server, client)
        local message = 'OAuth2 on port ' .. server.port .. ' accepted client from ' .. client.host .. ':' .. client.port
        logger(logger.TRACE, message)
        if instance.on_accept then
            instance:on_accept(client)
        end
    end

    instance.server.on_close = function(server, client)
        local message = 'OAuth2 on port ' .. server.port .. ' closed connection to client ' .. client.host .. ':' .. client.port
        logger(logger.TRACE, message)
        if instance.on_close then
            instance:on_close(client)
        end
    end

    instance.server.on_receive = function(sock, client, data)
        client.buffer = (client.buffer or '') .. data

        local ok, http, err = util.parse_http_message(client.buffer)
        if ok then
            local expected_length = tonumber(http.headers['Content-Length']) or -1
            if expected_length > 0 and #http.body < expected_length then
                return
            end

            local code = '200 OK'
            local ctype = 'text/plain'
            local body

            if http.path == '/' then
                if http.method == 'GET' then
                    body = util.oauth2_page
                    ctype = 'text/html'
                elseif http.method == 'POST' then
                    local reply
                    if http.headers['Content-Type'] == 'application/json; charset=UTF-8' then
                        local params = util.from_json(http.body)
                        local callback_ok, callback_result = pcall(instance.on_auth_callback, instance, params)
                        if not callback_ok or not callback_result then
                            code = '500 Internal Server Error'
                            reply = { granted = false, message = callback_result }
                            logger(logger.ERROR, 'OAuth2 callback failed: ' .. callback_result)
                        else
                            reply = callback_result
                        end
                    else
                        reply = {
                            granted = false,
                            message = 'Browser error: invalid Content-Type in message (expected: json)',
                        }
                    end
                    body = util.to_json(reply, false)
                    ctype = 'application/json'
                    instance.server.enabled = false
                    instance.promise:fulfill(false)
                else
                    code = '405 Method Not Allowed'
                    body = 'Method not allowed'
                end
            elseif http.path == '/favicon.svg' then
                if http.method == 'GET' then
                    body = util.svg_icon
                    ctype = 'image/svg+xml'
                else
                    code = '405 Method Not Allowed'
                    body = 'Method not allowed'
                end
            else
                code = '404 Not Found'
                body = 'Content not found'
            end

            local parts = {
                'HTTP/1.1 ' .. code,
                'Server: deck-assistant',
                'Content-Type: ' .. ctype,
                'Content-Length: ' .. #body,
                '',
                body,
            }
            local reply = table.concat(parts, '\r\n')
            client:send(reply)
            client:close()
        elseif err then
            logger(logger.WARNING, 'OAuth2: invalid HTTP request: ' .. err)
            client:close()
        end
    end

    instance.initial_setup = function(self)
        assert(#self.ports > 0)
        self.port_index = 1
        self.server.port = self.ports[1]
    end

    instance.tick_inputs = function(self, clock)
        self.server:tick_inputs(clock)
    end

    instance.tick_outputs = function(self)
        self.server:tick_outputs()
    end

    instance.shutdown = function(self)
        self.server:shutdown()
    end

    instance.on_auth_callback = function(self, params)
        local reply = {
            granted = false,
            message = 'OAuth2.on_auth_callback not implemented',
        }
        return reply
    end

    instance.wait = function(self)
        return instance.promise:wait()
    end

    return instance
end

deck.connector_factory.OAuth2 = oauth2_connector

local oauth2_twitch_connector = function()
    local instance = oauth2_connector()

    instance.client_id = 'dgmscqylvzur6u2ww0qykq4f3j9zmx'
    instance.scopes = {
        'moderator:read:followers',
        'channel:read:subscriptions',
        'channel:read:redemptions',
        'channel:read:hype_train',
        'bits:read',
    }

    instance.on_connect = function(self, port)
        instance.nonce = util.to_hex(util.random_bytes(8))

        local url = {
            'https://id.twitch.tv/oauth2/authorize?client_id=',
            instance.client_id,
            '&redirect_uri=http://localhost:',
            port,
            '&response_type=token',
            '&state=',
            instance.nonce,
            '&scope=',
            table.concat(instance.scopes, '+')
        }

        if instance.force_verify then
            table.insert(url, '&force_verify=true')
        end

        local target = table.concat(url)
        logger(logger.DEBUG, 'TwitchOAuth2: opening browser to: ' .. target)
        util.open_browser(target)
    end

    instance.on_auth_callback = function(self, params)
        local reply = {
            granted = false,
        }
        if params.state ~= instance.nonce then
            reply.message = 'Invalid STATE nonce, reply ignored'
        elseif params.error_description then
            reply.message = 'OAuth error: ' .. params.error_description
        elseif params.error then
            reply.message = 'OAuth error: ' .. params.error
        elseif params.token_type ~= 'bearer' then
            reply.message = 'Invalid TOKEN TYPE reply'
        elseif not params.access_token then
            reply.message = 'No ACCESS TOKEN in reply'
        else
            reply.granted = true
            reply.scopes = util.split(params.scope, ' ', true)

            instance.access_token = params.access_token
            instance.access_scopes = reply.scopes
            instance.promise:fulfill({ token = instance.access_token, scopes = instance.access_scopes })
            logger(logger.DEBUG, 'Got Twitch access token: ' .. instance.access_token)

            if instance.on_access_token then
                pcall(instance.on_access_token, instance, instance.access_token, instance.access_scopes)
            end
        end
        return reply
    end

    return instance
end

deck.connector_factory.TwitchOAuth2 = oauth2_twitch_connector
