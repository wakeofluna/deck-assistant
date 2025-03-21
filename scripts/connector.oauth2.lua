local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')

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

return {}