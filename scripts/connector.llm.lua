local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')

local llm_connector = function()
    local instance = {}

    instance.enabled = true
    instance.system = 'You are a chatbot, concise and to the point.'
    instance.tools = {}
    instance.tool_functions = {}
    instance.structured_formats = {}
    instance.structured_format_roles = {}
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

    instance._format_structured_format = function(parameters)
        if not parameters then
            return
        end

        if type(parameters) ~= 'table' then
            logger(logger.ERROR, 'Parameters must be a table')
            return
        end

        if not parameters.properties then
            parameters = { properties = parameters }
        end

        if not parameters.type then
            parameters.type = 'object'
        end

        for key, value in pairs(parameters.properties) do
            if type(value) == 'string' then
                parameters.properties[key] = { type = value }
            end
        end

        if not parameters.required then
            local required = {}
            for key, _ in pairs(parameters.properties) do
                table.insert(required, key)
            end
            parameters.required = required
        end

        return parameters
    end

    instance._format_messages = function(self, history, system_role)
        local messages = {}

        if type(history) == 'string' then
            table.insert(messages, { role = 'user', content = history })
        elseif type(history) == 'table' then
            for _, entry in ipairs(history) do
                if type(entry) == 'string' then
                    table.insert(messages, { role = 'user', content = entry })
                else
                    table.insert(messages, entry)
                end
            end
        else
            logger(logger.ERROR, "Chat history must be table-array or string")
            return {}
        end

        if system_role then
            local _, first = next(messages)
            if first and first.role ~= 'system' then
                table.insert(messages, 1, { role = 'system', content = system_role })
            end
        end

        return messages
    end

    instance._format_tools = function(self, enabled_tools)
        local tools = {}

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
        end
        if next(tools) == nil then
            tools = nil
        end

        return tools
    end

    instance.add_structured_format = function(self, name, system_role, schema)
        self.structured_formats[name] = self._format_structured_format(schema)
        self.structured_format_roles[name] = system_role
    end

    instance.add_tool = function(self, tool_name, description, parameters, callback)
        if type(description) ~= 'string' then
            logger(logger.ERROR, 'Tool description must be a string')
            return
        end

        if type(callback) ~= 'function' then
            logger(logger.ERROR, 'Callback must be a function')
            return
        end

        parameters = self._format_structured_format(parameters)

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

    instance._extract_message_from_reply = function(reply)
        if reply.content then
            return reply.content
        elseif reply.choices then
            return reply.choices[1].message
        else
            return reply.message
        end
    end

    instance._run_chat_request = function(self, endpoint, post_args)
        local messages = post_args.messages
        while true do
            local promise = self.http:post(endpoint, post_args)
            local result = promise:wait()
            if result.ok then
                local body = util.from_json(result.body)
                if body.error then
                    return false, body.error
                end
                local message = self._extract_message_from_reply(body)
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
                                if type(toolargs) == 'string' then
                                    toolargs = util.from_json(toolargs)
                                end
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


local ollama_connector = function()
    local instance = llm_connector()

    instance.http = deck:Connector('Http', 'OllamaApi', { base_url = 'http://127.0.0.1:11434/api/', timeout = 5000 })

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
            logger(logger.ERROR, "No model selected for Ollama connector")
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

    instance.chat = function(self, history, enabled_tools, output_format)
        if not self.model then
            logger(logger.ERROR, "No model selected for Ollama")
            return false, 'No model selected'
        end

        local messages = self:_format_messages(history, self.structured_format_roles[output_format] or self.system)
        local tools = self:_format_tools(enabled_tools)
        local args = {
            model = self.model,
            messages = messages,
            tools = tools,
            options = self.options,
            stream = false
        }

        local format = self.structured_formats[output_format]
        if format then
            args.format = format
        elseif output_format then
            logger(logger.ERROR, "Unknown/invalid structured format: " .. output_format)
        end

        local ok, result = self:_run_chat_request('/chat', args)
        if ok and format then
            result = util.from_json(result)
        end
        return ok, result
    end

    return instance
end


local llama_connector = function()
    local instance = llm_connector()

    instance.http = deck:Connector('Http', 'LlamaApi', { base_url = 'http://127.0.0.1:8080/', timeout = 5000 })

    instance._format_chat_args = function(self, history, enabled_tools, system_role)
        local args = {}
        for k, v in pairs(self.options) do
            args[k] = v
        end
        args.messages = self:_format_messages(history, system_role)
        args.tools = self:_format_tools(enabled_tools)
        return args
    end

    instance.create_prompt = function(self, history, enabled_tools)
        local args = self:_format_chat_args(history, enabled_tools, self.system)
        local promise = self.http:post('/apply-template', args)
        local result = promise:wait()
        if result.ok then
            local body = util.from_json(result.body)
            if body.prompt then
                return true, body.prompt
            elseif body.error then
                return false, body.error.message
            else
                return false, 'Unknown/invalid response from server'
            end
        else
            return false, result.error
        end
    end

    instance.generate_response = function(self, prompt)
        local args = {
            prompt = prompt,
            stream = false,
        }
        for k, v in pairs(self.options) do
            args[k] = v
        end

        local promise = self.http:post('/completion', args)
        local result = promise:wait()
        if result.ok then
            local body = util.from_json(result.body)
            return true, body.content
        else
            return false, result.error
        end
    end

    instance.chat = function(self, history, enabled_tools, output_format)
        local args = self:_format_chat_args(history, enabled_tools, self.structured_format_roles[output_format] or self.system)

        local format = self.structured_formats[output_format]
        if format then
            args.response_format = {
                type = 'json_object',
                schema = format
            }
        elseif output_format then
            logger(logger.ERROR, "Unknown/invalid structured format: " .. output_format)
        end

        local ok, result = self:_run_chat_request('/v1/chat/completions', args)
        if ok and format then
            result = util.from_json(result)
        end
        return ok, result
    end

    return instance
end

deck.connector_factory.Ollama = ollama_connector
deck.connector_factory.Llama = llama_connector
deck.connector_factory.LlamaCpp = llama_connector

return {}
