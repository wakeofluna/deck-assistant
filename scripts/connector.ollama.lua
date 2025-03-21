local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')

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

return {}