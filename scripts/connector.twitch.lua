local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')
require('connector.oauth2')

local twitch_client_id = 'dgmscqylvzur6u2ww0qykq4f3j9zmx'

local oauth2_twitch_connector = function()
    local instance = deck.connector_factory.OAuth2()

    instance.client_id = twitch_client_id
    instance.scopes = {}

    instance.on_connect = function(self, port)
        self.nonce = util.to_hex(util.random_bytes(8))

        local url = {
            'https://id.twitch.tv/oauth2/authorize?client_id=',
            self.client_id,
            '&redirect_uri=http://localhost:',
            port,
            '&response_type=token',
            '&state=',
            self.nonce,
            '&scope=',
            table.concat(self.scopes, '+')
        }

        if self.force_verify then
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
    instance._user_cache = {}

    instance.enabled = true
    instance.secret_name = 'twitch_access_token'
    instance.client_id = twitch_client_id

    instance.scopes = {
        follower_data = false,
        subscriber_data = false,
        redemption_notifications = false,
        bits_notifications = false,
        view_chat = false,
        send_chat = false,
        send_shoutouts = false,
        send_announcements = false,
        automod = false,
        moderate = false,
    }

    instance.active_subscriptions = {}

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
    instance._state_msg[instance.SUBSCRIBING] = 'initial subscribing phase'
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

        local scopes = {}
        if self.scopes.follower_data then table.insert(scopes, 'moderator:read:followers') end
        if self.scopes.subscriber_data then table.insert(scopes, 'channel:read:subscriptions') end
        if self.scopes.redemption_notifications then table.insert(scopes, 'channel:read:redemptions') end
        if self.scopes.bits_notifications then table.insert(scopes, 'bits:read') end
        if self.scopes.view_chat then table.insert(scopes, 'user:read:chat') end
        if self.scopes.send_chat then table.insert(scopes, 'user:write:chat') end
        if self.scopes.send_shoutouts then table.insert(scopes, 'moderator:manage:shoutouts') end
        if self.scopes.send_announcements then table.insert(scopes, 'moderator:manage:announcements') end
        if self.scopes.automod then table.insert(scopes, 'moderator:manage:automod') end
        if self.scopes.moderate then
            table.insert(scopes, 'moderator:manage:banned_users')
            table.insert(scopes, 'channel:moderate')
        end

        local oauth = deck:Connector('TwitchOAuth2', { enabled = false })
        oauth.client_id = self.client_id
        oauth.scopes = scopes
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

    instance.get_state = function(self)
        return self._internal_state
    end

    instance.subscribe = function(self, event, version, condition)
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
                self.active_subscriptions[body.data[1].id] = body.data[1]
                logger(logger.INFO, 'Twitch: subscribed to event ' .. event)
                return true
            else
                logger(logger.WARNING, 'Twitch: unable to subscribe to event ' .. event .. ': ' .. body.message)
                return false, body.message
            end
        else
            logger(logger.WARNING, 'Twitch: failed to subscribe to event ' .. event .. ': ' .. result.error)
            return false, result.error
        end
    end

    instance.subscribe_all = function(self, broadcaster_user_id)
        broadcaster_user_id = broadcaster_user_id or self.user_id

        local succeeded = {}
        local failed = {}

        local function do_subscribe(event, version, condition)
            version = version or '1'
            condition = condition or {}
            condition.broadcaster_user_id = broadcaster_user_id
            local ok, err = self:subscribe(event, version, condition)
            if ok then
                table.insert(succeeded, event)
            else
                failed[event] = err
            end
        end

        logger(logger.INFO, 'Twitch: subscribing to broadcaster ' .. broadcaster_user_id)

        if self.scopes.follower_data then
            do_subscribe('channel.follow', 2, { moderator_user_id = self.user_id })
        end
        if self.scopes.bits_notifications then
            do_subscribe('channel.bits.use')
        end
        if self.scopes.redemption_notifications then
            do_subscribe('channel.channel_points_custom_reward_redemption.add')
        end
        if self.scopes.view_chat and broadcaster_user_id == self.user_id then
            do_subscribe('channel.chat.notification', 1, { user_id = self.user_id })
        elseif self.scopes.subscriber_data then
            do_subscribe('channel.subscribe')
            do_subscribe('channel.subscription.end')
            do_subscribe('channel.subscription.gift')
            do_subscribe('channel.subscription.message')
        end
        if self.scopes.moderate then
            do_subscribe('channel.ban')
            do_subscribe('channel.unban')
        end

        return succeeded, failed
    end

    instance.subscribe_chat = function(self, broadcaster_user_id)
        if not self.scopes.view_chat then
            return false, "missing view_chat scope"
        end

        broadcaster_user_id = broadcaster_user_id or self.user_id

        local function do_subscribe(event, version, as_moderator)
            version = version or '1'
            local condition = {}
            condition.broadcaster_user_id = broadcaster_user_id
            if as_moderator then
                condition.moderator_user_id = self.user_id
            else
                condition.user_id = self.user_id
            end
            return self:subscribe(event, version, condition)
        end

        logger(logger.INFO, 'Twitch: entering chat of broadcaster ' .. broadcaster_user_id)

        local ok, err = do_subscribe('channel.chat.message')
        if not ok then
            return ok, err
        end

        do_subscribe('channel.chat.message_delete')
        do_subscribe('channel.chat.clear')
        do_subscribe('channel.chat.clear_user_messages')

        if broadcaster_user_id ~= self.user_id then
            do_subscribe('channel.chat.notification')
        end

        if self.scopes.automod then
            ok = do_subscribe('automod.message.hold', 2, true)
            if ok then
                do_subscribe('automod.message.update', 2, true)
            end
        end

        return true
    end

    instance.unsubscribe = function(self, event, broadcaster_user_id)
        local found = false
        for key, data in pairs(self.active_subscriptions) do
            if data.type == event and (broadcaster_user_id == nil or broadcaster_user_id == data.condition.broadcaster_user_id) then
                logger(logger.INFO, 'Twitch: unsubscribing ' .. event)
                self._api:delete('/eventsub/subscriptions?id=' .. data.id)
                self.active_subscriptions[key] = nil
                found = true
            end
        end
        return found
    end

    instance.unsubscribe_all = function(self, broadcaster_user_id)
        if broadcaster_user_id then
            logger(logger.INFO, 'Twitch: unsubscribing from broadcaster ' .. broadcaster_user_id)
        else
            logger(logger.INFO, 'Twitch: unsubscribing all subscriptions')
        end

        for key, data in pairs(self.active_subscriptions) do
            if broadcaster_user_id == nil or broadcaster_user_id == data.condition.broadcaster_user_id then
                logger(logger.INFO, 'Twitch: unsubscribed from event ' .. data.type)
                self._api:delete('/eventsub/subscriptions?id=' .. data.id)
                self.active_subscriptions[key] = nil
            end
        end
    end

    instance.unsubscribe_chat = function(self, broadcaster_user_id)
        broadcaster_user_id = broadcaster_user_id or self.user_id

        logger(logger.INFO, 'Twitch: leaving chat of broadcaster ' .. broadcaster_user_id)

        self:unsubscribe('channel.chat.message', broadcaster_user_id)
        self:unsubscribe('channel.chat.message_delete', broadcaster_user_id)
        self:unsubscribe('channel.chat.clear', broadcaster_user_id)
        self:unsubscribe('channel.chat.clear_user_messages', broadcaster_user_id)
        if broadcaster_user_id ~= self.user_id then
            self:unsubscribe('channel.chat.notification', broadcaster_user_id)
        end
        self:unsubscribe('automod.message.hold', broadcaster_user_id)
        self:unsubscribe('automod.message.update', broadcaster_user_id)
    end

    instance.resolve_user = function(self, user)
        user = string.lower(user)

        if self._user_cache[user] then
            return self._user_cache[user]
        end

        if self.debugging then
            coroutine.yield()
            local result = {}
            result.login = 'testViewer'
            result.id = '73612018'
            return result
        end

        local promise
        if tonumber(user) then
            promise = self._api:get('/users?id=' .. user)
        else
            promise = self._api:get('/users?login=' .. user)
        end

        local result = promise:wait()
        if result.ok then
            local body = util.from_json(result.body) or {}
            for _, data in ipairs(body.data) do
                logger(logger.INFO, 'Twitch: resolved user', user, 'to', data.id, '/', data.login)
                self._user_cache[user] = data
                self._user_cache[data.id] = data
                self._user_cache[data.login] = data
            end
        end

        return self._user_cache[user]
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
            self.api_id = payload.session.id
            self.api_transport = { method = 'websocket', session_id = payload.session.id }
            self.api_keepalive = payload.session.keepalive_timeout_seconds

            self:_update_state(self.SUBSCRIBING)

            -- Give the user a chance to do their own subscribing
            if self.on_initial_subscribe then
                self:on_initial_subscribe()
            else
                self:subscribe_all()
                self:subscribe_chat()
            end

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
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                notify('on_follow', event.broadcaster_user_name, user)
            elseif subtype == 'channel.subscribe' then
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                notify('on_subscribe', event.broadcaster_user_name, user, event.tier, event.is_gift)
            elseif subtype == 'channel.subscription.end' then
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                notify('on_unsubscribe', event.broadcaster_user_name, user, event.tier, event.is_gift)
            elseif subtype == 'channel.subscription.gift' then
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                notify('on_subscription_gift', event.broadcaster_user_name, user, event.tier, event.total, event.cumulative_total)
            elseif subtype == 'channel.subscription.message' then
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                notify('on_resubscribe', event.broadcaster_user_name, user, event.tier, event.cumulative_months, event.streak_months, event.message.text)
            elseif subtype == 'channel.bits.use' then
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                local event_type = event.type
                if event_type == 'power_up' then
                    event_type = event.power_up.type
                end
                notify('on_bits', event.broadcaster_user_name, user, event.bits, event_type, event.message)
            elseif subtype == 'channel.channel_points_custom_reward_redemption.add' then
                local user = { id = event.user_id, name = event.user_name, login = event.user_login }
                notify('on_channel_points', event.broadcaster_user_name, user, event.reward.cost, event.reward.title, event.user_input)
            elseif subtype == 'channel.chat.clear' then
                notify('on_channel_chat_clear', event.broadcaster_user_name)
            elseif subtype == 'channel.chat.message' then
                local chatter = { id = event.chatter_user_id, name = event.chatter_user_name, login = event.chatter_user_login, color = event.color }
                local extra = { cheer = event.cheer, reply = event.reply, redeem = event.channel_points_custom_reward_id }
                if event.source_broadcaster_user_id then
                    extra.source = { id = event.source_broadcaster_user_id, name = event.source_broadcaster_user_name, login = event.source_broadcaster_user_login }
                end
                notify('on_channel_chat_message', event.broadcaster_user_name, event.message_id, chatter, event.message, extra)
            elseif subtype == 'channel.chat.message_delete' then
                local target = { id = event.target_user_id, name = event.target_user_name, login = event.target_user_login }
                notify('on_channel_chat_message_delete', event.broadcaster_user_name, event.message_id, target)
            elseif subtype == 'channel.chat.clear_user_messages' then
                local target = { id = event.target_user_id, name = event.target_user_name, login = event.target_user_login }
                notify('on_channel_chat_message_delete', event.broadcaster_user_name, nil, target)
            elseif subtype == 'channel.chat.notification' then
                notify('on_channel_chat_notification', event.broadcaster_user_name, event)
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
        instance.active_subscriptions = {}
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

return {}
