local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')

local function obs_connector_lowlevel()
    local instance = {}

    instance.promises = deck:PromiseList()

    instance.enabled = true
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
        self.socket.enabled = self.enabled
        self.socket:tick_inputs(clock)

        if self.handshaking and clock > self.handshaking + 2000 then
            local err = 'Did not receive handshake from OBS within time limit'
            logger(logger.ERROR, err)

            self.handshaking = false
            self.socket.enabled = false
            if self.on_connect_failed then
                self:on_connect_failed(err)
            end
        end
    end

    instance.tick_outputs = function(self, clock)
        self.socket:tick_outputs(clock)
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
        self.socket:write(util.to_json(request))
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
            logger(logger.TRACE, 'OBS.' .. event .. ': ' .. util.to_json(data, true))
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

    instance.enabled = true
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
        instance.promise:reset()
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
    end

    instance.server.on_close = function(server, client)
        local message = 'OAuth2 on port ' .. server.port .. ' closed connection to client ' .. client.host .. ':' .. client.port
        logger(logger.TRACE, message)
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
                    ctype = 'application/json; charset=UTF-8'
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
        self.server.enabled = self.enabled
        self.server:tick_inputs(clock)
    end

    instance.tick_outputs = function(self, clock)
        self.server:tick_outputs(clock)
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
        return self.promise:wait()
    end

    return instance
end

deck.connector_factory.OAuth2 = oauth2_connector

local twitch_client_id = 'dgmscqylvzur6u2ww0qykq4f3j9zmx'

local oauth2_twitch_connector = function()
    local instance = oauth2_connector()

    instance.client_id = twitch_client_id
    instance.scopes = {
        'moderator:read:followers',
        'channel:read:subscriptions',
        'channel:read:redemptions',
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
        logger(logger.TRACE, 'TwitchOAuth2: opening browser to: ' .. target)
        util.open_browser(target)
    end

    instance.on_disconnect = function(self, msg)
        self.promise:fulfill(false)
    end

    instance.on_auth_callback = function(self, params)
        local reply = {
            granted = false,
        }
        if params.state ~= self.nonce then
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

            self.access_token = params.access_token
            self.access_scopes = reply.scopes
            self.promise:fulfill({ token = self.access_token, scopes = self.access_scopes })
            logger(logger.TRACE, 'TwitchOAuth2: Got access token: ' .. self.access_token)

            if self.on_access_token then
                pcall(self.on_access_token, self, self.access_token, self.access_scopes)
            end
        end
        if not reply.granted then
            logger(logger.WARNING, 'TwitchOAuth2: ' .. reply.message)
        end
        return reply
    end

    return instance
end

deck.connector_factory.TwitchOAuth2 = oauth2_twitch_connector


local twitch_connector = function()
    local instance = {}

    instance.debugging = false

    if instance.debugging then
        instance._api = deck:Connector('Http', 'TwitchApi', { base_url = 'http://127.0.0.1:8080/' })
        instance._ws = deck:Connector('Websocket', 'TwitchWS', { connection_string = 'ws://127.0.0.1:8080/ws', enabled = false })
    else
        instance._api = deck:Connector('Http', 'TwitchApi', { base_url = 'https://api.twitch.tv/helix/' })
        instance._ws = deck:Connector('Websocket', 'TwitchWS', { connection_string = 'wss://eventsub.wss.twitch.tv/ws', enabled = false })
    end
    instance._promise = deck:Promise(2000)

    instance.enabled = true
    instance.secret_name = 'twitch_access_token'
    instance.client_id = twitch_client_id

    instance.INACTIVE = 89172634
    instance.AWAITING_WELCOME = 109702145
    instance.GETTING_TOKEN = 987123
    instance.VALIDATING_TOKEN = 1248709
    instance.CONNECTING = 3155611
    instance.SUBSCRIBING = 44112356
    instance.ACTIVE = 55190901
    instance._internal_state = instance.INACTIVE

    instance._state_msg = {}
    instance._state_msg[instance.INACTIVE] = 'inactive'
    instance._state_msg[instance.AWAITING_WELCOME] = 'awaiting welcome from server'
    instance._state_msg[instance.GETTING_TOKEN] = 'requesting user access token'
    instance._state_msg[instance.VALIDATING_TOKEN] = 'validating user access token'
    instance._state_msg[instance.CONNECTING] = 'connecting to api endpoint'
    instance._state_msg[instance.SUBSCRIBING] = 'subscribing to required events'
    instance._state_msg[instance.ACTIVE] = 'active'

    instance._update_state = function(self, newstate)
        if self._internal_state ~= newstate then
            local msg = assert(self._state_msg[newstate], 'Twitch: INTERNAL STATE INVALID')
            local logmsg = 'Twitch: ' .. msg

            self._internal_state = newstate

            if self.on_state_changed then
                logger(logger.DEBUG, logmsg)
                util.ycall(self.on_state_changed, self, newstate, msg)
            else
                logger(logger.INFO, logmsg)
            end
        end
    end

    instance._validate_token = function(self, token)
        if token then
            self:_update_state(self.VALIDATING_TOKEN)

            if self.debugging then
                coroutine.yield()
                local result = {}
                result.client_id = self.twitch_client_id
                result.login = 'testBroadcaster'
                result.user_id = '99532582'
                result.expires_in = 5279237
                result.scopes = { 'bits.read' }
                result.token = token
                return result
            end

            local idserver = deck:Connector('Http', 'TwitchIdServer', { base_url = 'https://id.twitch.tv/oauth2/' })
            local promise = idserver:get('/validate', { Authorization = 'OAuth ' .. token })
            local result = promise:wait()

            if result.ok then
                local body = util.from_json(result.body)
                if result.code == 200 then
                    body.token = token
                    logger(logger.INFO, 'Twitch: token validated for user ' .. body.login .. ', expires in ' .. body.expires_in .. ' seconds')
                    return body
                else
                    logger(logger.DEBUG, 'Twitch: token validation failed: ' .. result.code .. ' ' .. body.message)
                end
            else
                logger(logger.WARNING, 'Twitch: token validation failed: ' .. result.error)
            end
        end
    end

    instance._request_token = function(self)
        self:_update_state(self.GETTING_TOKEN)
        logger(logger.DEBUG, 'Twitch: asking user for access token')

        local oauth = deck:Connector('TwitchOAuth2', { enabled = false })
        oauth.client_id = self.client_id
        oauth.enabled = true
        local result = oauth:wait()
        oauth.enabled = false

        if result then
            return self:_validate_token(result.token)
        else
            return
        end
    end

    instance._do_connect = function(self)
        local access_token = util.retrieve_secret(self.secret_name)

        if access_token then
            access_token = self:_validate_token(access_token)
            if not access_token then
                util.store_secret(self.secret_name, '')
            end
        end

        if not access_token then
            access_token = self:_request_token()
            if access_token then
                util.store_secret(self.secret_name, access_token.token)
            else
                self:_update_state(self.INACTIVE)
                self.enabled = false
                logger(logger.WARNING, 'Twitch: unable to validate access token')
                return
            end
        end

        self._api:set_header('Authorization', 'Bearer ' .. access_token.token)
        self._api:set_header('Client-Id', self.client_id)
        self.user_login = access_token.login
        self.user_id = access_token.user_id

        self:_update_state(self.CONNECTING)
        self._ws.enabled = true
    end

    instance._do_subscribe = function(self, event, version, condition)
        assert(event, "no event type in Twitch subscribe")
        assert(version, "no event version in Twitch subscribe")

        local data = {}
        data.type = event
        data.version = tostring(version)
        data.condition = condition
        data.transport = self.api_transport

        local promise = self._api:post('/eventsub/subscriptions', data)
        local result = promise:wait()

        if result.ok then
            local body = util.from_json(result.body)
            if result.code >= 200 and result.code < 300 then
                return true
            else
                return false, body.message
            end
        else
            return false, result.error
        end
    end

    instance.tick_inputs = function(self, clock)
        if not self.enabled then
            self._ws.enabled = false
            self:_update_state(self.INACTIVE)
        elseif self._internal_state == self.INACTIVE then
            util.ycall(self._do_connect, self)
        end
    end

    instance.tick_outputs = function(self, clock)
    end

    instance.shutdown = function(self)
    end

    instance._on_message = function(self, metadata, payload)
        self.last_message = deck.clock

        local metatype = metadata.message_type

        if metatype == 'session_welcome' then
            self:_update_state(instance.SUBSCRIBING)

            self.api_id = payload.session.id
            self.api_transport = { method = 'websocket', session_id = payload.session.id }
            self.api_keepalive = payload.session.keepalive_timeout_seconds

            local function do_subscribe(event, version, condition)
                version = version or '1'
                condition = condition or { broadcaster_user_id = self.user_id }

                local ok, err = self:_do_subscribe(event, version, condition)
                if ok then
                    logger(logger.INFO, 'Twitch: subscribed to event ' .. event)
                else
                    logger(logger.WARNING, 'Twitch: unable to subscribe to event ' .. event .. ': ' .. err)
                end
            end

            do_subscribe('channel.follow', 2, { broadcaster_user_id = self.user_id, moderator_user_id = self.user_id })
            do_subscribe('channel.subscribe')
            do_subscribe('channel.subscription.end')
            do_subscribe('channel.subscription.gift')
            do_subscribe('channel.subscription.message')
            do_subscribe('channel.cheer')
            do_subscribe('channel.channel_points_custom_reward_redemption.add')

            self:_update_state(self.ACTIVE)
        elseif metatype == 'session_keepalive' then
            -- ignored
        elseif metatype == 'notification' then
            local subtype = metadata.subscription_type
            local event = payload.event or {}

            local function notify(event, ...)
                local msg = 'Twitch: ' .. event .. ':'
                local func = self[event]
                if func then
                    logger(logger.DEBUG, msg, ...)
                    func(self, ...)
                else
                    logger(logger.INFO, msg, ...)
                end
            end

            if subtype == 'channel.follow' then
                notify('on_follow', event.broadcaster_user_name, event.user_name, event.user_id)
            elseif subtype == 'channel.subscribe' then
                notify('on_subscribe', event.broadcaster_user_name, event.user_name, event.user_id, event.tier, event.is_gift)
            elseif subtype == 'channel.subscription.end' then
                notify('on_unsubscribe', event.broadcaster_user_name, event.user_name, event.user_id, event.tier, event.is_gift)
            elseif subtype == 'channel.subscription.gift' then
                notify('on_subscription_gift', event.broadcaster_user_name, event.user_name, event.user_id, event.tier, event.total, event.cumulative_total)
            elseif subtype == 'channel.subscription.message' then
                notify('on_resubscribe', event.broadcaster_user_name, event.user_name, event.user_id, event.tier, event.cumulative_months, event.streak_months, event.message.text)
            elseif subtype == 'channel.cheer' then
                notify('on_cheer', event.broadcaster_user_name, event.user_name, event.user_id, event.bits, event.message)
            elseif subtype == 'channel.channel_points_custom_reward_redemption.add' then
                notify('on_channel_points', event.broadcaster_user_name, event.user_name, event.user_id, event.reward.cost, event.reward.title, event.user_input)
            else
                logger(logger.WARNING, 'Twitch: unhandled notification of type ' .. subtype .. ': ' .. util.to_json(payload, true))
            end
        elseif metatype == 'session_reconnect' then
            local session = payload.session or {}
            logger(logger.WARNING, 'Twitch: ' .. metatype .. ' handler not implemented, reconnect_url = ' .. session.reconnect_url)
        elseif metatype == 'revocation' then
            local subscription = payload.subscription or {}
            logger(logger.ERROR, 'Twitch: subscription revoked: ' .. subscription.type .. ' with reason: ' .. subscription.status)
        else
            logger(logger.WARNING, 'Twitch: ' .. metatype .. ' handler not implemented, payload = ' .. util.to_json(payload, true))
        end
    end

    instance._ws.on_connect = function(ws)
        instance:_update_state(instance.AWAITING_WELCOME)
    end

    instance._ws.on_disconnect = function(ws, msg)
        if instance.enabled then
            instance:_update_state(instance.CONNECTING)
        else
            instance:_update_state(instance.INACTIVE)
        end
    end

    instance._ws.on_message = function(ws, msg)
        local data = util.from_json(msg)
        --print('TWITCH WS: ' .. util.to_json(data, true))
        instance:_on_message(data.metadata or {}, data.payload or {})
    end

    return instance
end

deck.connector_factory.Twitch = twitch_connector


local ollama_connector = function()
    local instance = {}

    instance.http = deck:Connector('Http', 'OllamaApi', { base_url = 'http://127.0.0.1:11434/api/', timeout = 5000 })
    instance.enabled = true
    instance.tools = {}
    instance.tool_functions = {}
    instance.options = {
        temperature = 0.8,
        min_p = 0.2,
        top_k = 40,
        top_p = 0.9
    }

    instance.initial_setup = function(self)
        if self.host then
            instance.http.host = self.host
        end
        if self.port then
            instance.http.port = self.port
        end
        if self.timeout then
            instance.http.timeout = self.timeout
        end
    end

    instance.tick_inputs = function(self, clock)
        self.http.enabled = self.enabled
        self.http:tick_inputs(clock)
    end

    instance.tick_outputs = function(self, clock)
        self.http:tick_outputs(clock)
    end

    instance.shutdown = function(self)
        self.http:shutdown()
    end

    instance.get_version = function(self)
        local promise = self.http:get('/version')
        local result = promise:wait()
        if result.ok then
            local body = util.from_json(result.body)
            return body.version
        else
            return 'Unknown'
        end
    end

    instance.list_models = function(self, running)
        local promise
        if running then
            promise = self.http:get('/ps')
        else
            promise = self.http:get('/tags')
        end
        local result = promise:wait()

        if result.ok then
            local body = util.from_json(result.body)
            local models = {}
            for _, tag in ipairs(body.models) do
                table.insert(models, tag.model)
            end
            table.sort(models)
            return models
        else
            return {}
        end
    end

    instance.set_model = function(self, model)
        self.model = model
    end

    instance.generate_response = function(self, prompt)
        if not self.model then
            logger(logger.ERROR, "No model selected for Ollama")
            return false, 'No model selected'
        end

        local args = {
            model = self.model,
            prompt = prompt,
            options = self.options,
            stream = false,
        }
        local promise = self.http:post('/generate', args)
        local result = promise:wait()
        if result.ok then
            local body = util.from_json(result.body)
            return true, body.response
        else
            return false, result.error
        end
    end

    instance.add_chat_tool = function(self, tool_name, description, parameters, callback)
        if type(description) ~= 'string' then
            logger(logger.ERROR, 'Tool description must be a string')
            return
        end

        if type(parameters) ~= 'table' then
            logger(logger.ERROR, 'Parameters must be a table')
            return
        end

        if type(callback) ~= 'function' then
            logger(logger.ERROR, 'Callback must be a function')
            return
        end

        if not parameters.properties then
            parameters = { properties = parameters }
        end

        if not parameters.type then
            parameters.type = 'object'
        end

        if not parameters.required then
            local required = {}
            for key, _ in pairs(parameters.properties) do
                table.insert(required, key)
            end
            parameters.required = required
        end

        local tool = {
            type = 'function',
            ['function'] = {
                name = tool_name,
                description = description,
                parameters = parameters,
            },
        }
        self.tools[tool_name] = tool
        self.tool_functions[tool_name] = callback
    end

    instance.get_tools = function(self)
        local tools = {}
        for key, _ in pairs(self.tools) do
            table.insert(tools, key)
        end
        table.sort(tools)
        return tools
    end

    instance.chat = function(self, history, enabled_tools)
        if not self.model then
            logger(logger.ERROR, "No model selected for Ollama")
            return false, 'No model selected'
        end

        local messages = {}
        local tools = {}

        if type(history) == 'string' then
            table.insert(messages, { role = 'user', content = history })
        elseif type(history) == 'table' then
            for _, entry in ipairs(history) do
                table.insert(messages, entry)
            end
        else
            logger(logger.ERROR, "Chat history must be table or string")
            return false, 'Invalid input for chat history'
        end

        if enabled_tools == nil then
            for _, def in pairs(self.tools) do
                table.insert(tools, def)
            end
        elseif type(enabled_tools) == 'string' then
            local def = self.tools[enabled_tools]
            if def then
                table.insert(tools, def)
            end
        elseif type(enabled_tools) == 'table' then
            for _, tool_name in ipairs(enabled_tools) do
                local def = self.tools[tool_name]
                if def then
                    table.insert(tools, def)
                end
            end
        else
            logger(logger.ERROR, "Enabled tools must be a table-array or string")
            return false, 'Invalid input for tool list'
        end
        if next(tools) == nil then
            tools = nil
        end

        local args = {
            model = self.model,
            messages = messages,
            tools = tools,
            options = self.options,
            stream = false
        }

        while true do
            local promise = self.http:post('/chat', args)
            local result = promise:wait()
            if result.ok then
                local body = util.from_json(result.body)
                if body.error then
                    return false, body.error
                end
                local message = body.message
                local tool_calls = message.tool_calls
                if type(tool_calls) == 'table' then
                    table.insert(messages, message)
                    local ok = false
                    for _, tool_call in ipairs(tool_calls) do
                        tool_call = tool_call['function']
                        if type(tool_call) == 'table' then
                            local toolname = tool_call.name
                            local toolargs = tool_call.arguments
                            local tool = self.tool_functions[toolname]
                            if type(tool) == 'function' then
                                local answer = tool(toolargs)
                                if answer then
                                    ok = true
                                    if type(answer) ~= 'string' then
                                        answer = util.to_json(answer)
                                    end
                                    table.insert(messages, { role = 'tool', content = tostring(answer) })
                                end
                            end
                        end
                    end
                    if not ok then
                        return false, 'Unable to satisfy tool request'
                    end
                else
                    return true, message.content
                end
            else
                return false, result.error
            end
        end
    end

    return instance
end

deck.connector_factory.Ollama = ollama_connector
