local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')
local builtins = require('deck.builtins')

builtins.default_font.size = 20

local MODE_COUNTDOWN = 'Countdown'
local MODE_STOPWATCH = 'Stopwatch'
local color_disabled = deck:Colour 'darkred'
local color_connecting = deck:Colour 'goldenrod'
local color_enabled = deck:Colour 'green'

local SETTINGS_TABLE_NAME = 'settings'
local SETTINGS = util.retrieve_table(SETTINGS_TABLE_NAME) or {}
SETTINGS.mode = SETTINGS.mode or MODE_COUNTDOWN
SETTINGS.countdown_time = SETTINGS.countdown_time or 60
SETTINGS.countdown_finished = SETTINGS.countdown_finished or 'Soon!'

obs = deck:Connector('OBS', 'Obs', { enabled = false })
main_window = deck:Connector('Window', 'MainWindow', { width = 300, height = 175 })
settings_window = deck:Connector('Window', 'SettingsWindow', { width = 850, height = 200, visible = false, exit_on_close = false })

local grid = builtins.create_grid(4, 3)
grid.bgcolor = deck:Colour 'Black'
builtins.connect(grid, main_window)

local start_button = builtins.create_button('Start')
grid:add_child(start_button, 0, 0)
local stop_button = builtins.create_button('Stop')
grid:add_child(stop_button, 0, 1)
local reset_button = builtins.create_button('Reset')
grid:add_child(reset_button, 0, 2)

local time_label = builtins.create_label('00:00:00')
time_label.alignment = ALIGN_CENTER
time_label.font = builtins.default_font:clone()
time_label.font.size = 48
grid:add_child(time_label, 1, 0, 2, grid.cols)

local obs_label = builtins.create_label('OBS')
obs_label.alignment = ALIGN_CENTER
obs_label.bgcolor = color_disabled
grid:add_child(obs_label, -1, 0)

local settings_button = builtins.create_button('Settings', function(self)
    settings_window.visible = true
end)
grid:add_child(settings_button, -1, -1)


local timer_value = 0
local clock_running = false

local function fractionalize_time(stamp)
    local hours = math.floor(stamp / 3600)
    stamp = stamp - hours * 3600
    local minutes = math.floor(stamp / 60)
    stamp = stamp - minutes * 60
    local seconds = math.floor(stamp)
    stamp = stamp - seconds
    return { hours, minutes, seconds, math.floor(stamp * 1000 + 0.5) }
end

local function render_outputs(force)
    local fraction = timer_value

    if SETTINGS.mode == MODE_COUNTDOWN then
        fraction = SETTINGS.countdown_time - fraction
        if fraction < 0 then
            fraction = 0
            clock_running = false
        end
    end

    local time_string
    if SETTINGS.mode == MODE_COUNTDOWN and fraction == 0 then
        time_string = SETTINGS.countdown_finished
    else
        local parts = fractionalize_time(fraction)

        if parts[1] > 0 then
            time_string = string.format('%d:%02d:%02d', parts[1], parts[2], parts[3])
        else
            time_string = string.format('%d:%02d', parts[2], parts[3])
        end
        if false then
            time_string = string.format('%s.%03d', time_string, parts[4])
        end
    end

    local is_changed = time_label.text ~= time_string

    if time_string == '' then
        time_label:set_text('-:--')
    else
        time_label:set_text(time_string)
    end

    if obs.connected and (is_changed or force) then
        if SETTINGS.obs_source then
            obs:SetInputSettings(SETTINGS.obs_source, { text = time_string })
        end
    end
end
local function start_function()
    clock_running = true
end
local function stop_function()
    clock_running = false
end
local function reset_function()
    timer_value = 0
    render_outputs()
end

start_button.callback = start_function
stop_button.callback = stop_function
reset_button.callback = reset_function
render_outputs()


obs.on_connect_failed = function(self, reason)
    logger(logger.WARNING, 'OBS connection failed: ' .. reason)
    obs_label:set_bgcolor(color_connecting)
end
obs.on_connect = function(self)
    logger(logger.INFO, 'OBS connected!')
    obs_label:set_bgcolor(color_enabled)
    self.connected = true
    render_outputs(true)
end
obs.on_disconnect = function(self)
    logger(logger.INFO, 'OBS disconnected!')
    obs_label:set_bgcolor(color_disabled)
    obs_label:redraw(true)
    self.connected = false
end

if SETTINGS.obs_password then
    obs.password = SETTINGS.obs_password
    obs.enabled = true
    obs_label:set_bgcolor(color_connecting)
end


local grid = builtins.create_grid(6, 3)
grid.bgcolor = deck:Colour 'Black'
grid.homogeneous = false
builtins.connect(grid, settings_window)

local setting_obs_password_label = builtins.create_label('OBS password')
grid:add_child(setting_obs_password_label, 0, 0)
local setting_obs_password_inp = builtins.create_input_field(SETTINGS.obs_password or '')
setting_obs_password_inp.input_length = 16
setting_obs_password_inp.on_blur = function(self)
    SETTINGS.obs_password = self.text
    util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
    obs.password = SETTINGS.obs_password
    obs.enabled = (obs.password ~= '')
end
grid:add_child(setting_obs_password_inp, 0, 1)
local setting_obs_password_paste = builtins.create_button('Paste', function(self)
    setting_obs_password_inp:set_text(util.clipboard_text())
    setting_obs_password_inp:on_blur()
end)
grid:add_child(setting_obs_password_paste, 0, 2)

local setting_obs_source_label = builtins.create_label('OBS timer source name')
grid:add_child(setting_obs_source_label, 1, 0)
local setting_obs_source_inp = builtins.create_input_field(SETTINGS.obs_source or '')
setting_obs_source_inp.input_length = 32
setting_obs_source_inp.on_blur = function(self)
    SETTINGS.obs_source = self.text
    util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
    render_outputs(true)
end
grid:add_child(setting_obs_source_inp, 1, 1)
local setting_obs_source_paste = builtins.create_button('Paste', function(self)
    setting_obs_source_inp:set_text(util.clipboard_text())
    setting_obs_source_inp:on_blur()
end)
grid:add_child(setting_obs_source_paste, 1, 2)

local setting_mode_label = builtins.create_label('Counter mode')
grid:add_child(setting_mode_label, 2, 0)
local setting_mode_value = builtins.create_label(SETTINGS.mode)
grid:add_child(setting_mode_value, 2, 1)
local setting_mode_swap = builtins.create_button('Swap Mode', function(self)
    if SETTINGS.mode == MODE_COUNTDOWN then
        SETTINGS.mode = MODE_STOPWATCH
    else
        SETTINGS.mode = MODE_COUNTDOWN
    end
    util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
    setting_mode_value:set_text(SETTINGS.mode)
    render_outputs(true)
end)
grid:add_child(setting_mode_swap, 2, 2)

local setting_countdown_time_label = builtins.create_label('Countdown time')
grid:add_child(setting_countdown_time_label, 3, 0)

local start_time_parts = fractionalize_time(SETTINGS.countdown_time)
local function add_time_component(box, idx, postfix, length)
    local inp = builtins.create_input_field(tostring(start_time_parts[idx]))
    inp.numerical = true
    inp.input_length = length
    inp.on_blur = function(self)
        start_time_parts[idx] = tonumber(self.text)
        SETTINGS.countdown_time = start_time_parts[1] * 3600 + start_time_parts[2] * 60 + start_time_parts[3] + start_time_parts[4] / 1000.0
        util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
        render_outputs(true)
    end
    box:add_child(inp)
    local lbl = builtins.create_label(postfix)
    box:add_child(lbl)
end
local hbox = builtins.create_hbox()
add_time_component(hbox, 1, 'h  ', 2)
add_time_component(hbox, 2, 'm  ', 2)
add_time_component(hbox, 3, 's  ', 2)
add_time_component(hbox, 4, 'msec  ', 3)
hbox:relayout()
grid:add_child(hbox, 3, 1)

local setting_countdown_finished_label = builtins.create_label('Text to display when countdown time reaches zero:')
grid:add_child(setting_countdown_finished_label, 4, 0, 1, grid.cols)
local setting_countdown_finished_inp = builtins.create_input_field(SETTINGS.countdown_finished)
setting_countdown_finished_inp.input_length = 48
setting_countdown_finished_inp.on_blur = function(self)
    SETTINGS.countdown_finished = self.text
    util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
    render_outputs(true)
end
grid:add_child(setting_countdown_finished_inp, 5, 0, 1, grid.cols)

local last_tick = 0
function tick(clock)
    local delta = clock - last_tick
    if clock_running then
        timer_value = timer_value + delta / 1000.0
        render_outputs()
    end
    last_tick = clock
end
