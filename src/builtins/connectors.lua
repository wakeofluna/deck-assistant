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
