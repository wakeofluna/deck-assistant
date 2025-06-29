local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')
local widgets = require('deck.widgets')
local enum = require('deck.enum')
require('connector.obs')
require('connector.twitch')

widgets.default_font.size = 20

--logger.min_level = logger.DEBUG
logger.on_message = function(level, message)
    local ts = os.date('[%F %T]')
    local prefix
    if level == logger.TRACE then
        prefix = '[TRACE]'
    elseif level == logger.DEBUG then
        prefix = '[DEBUG]'
    elseif level == logger.INFO then
        prefix = '[INFO] '
    elseif level == logger.WARNING then
        prefix = '[WARN] '
    elseif level == logger.ERROR then
        prefix = '[ERROR]'
    else
        prefix = '       '
    end
    local msg = table.concat({ ts, prefix, message }, ' ')
    util.append_event_log('app', msg)
end

local SETTINGS_TABLE_NAME = 'settings'
local STATS_TABLE_NAME = 'stats'
local SETTINGS = {}
local STATS = {}

local function reload_settings_table()
    SETTINGS = util.retrieve_table(SETTINGS_TABLE_NAME) or {}
    SETTINGS.event_start_offset = SETTINGS.event_start_offset or 3600
    SETTINGS.seconds_per_sub_t1 = SETTINGS.seconds_per_sub_t1 or (60 * 15)
    SETTINGS.seconds_per_sub_t2 = SETTINGS.seconds_per_sub_t2 or (60 * 30)
    SETTINGS.seconds_per_sub_t3 = SETTINGS.seconds_per_sub_t3 or (60 * 45)
    SETTINGS.seconds_per_bits = SETTINGS.seconds_per_bits or (60 * 5)
    SETTINGS.seconds_per_donation = SETTINGS.seconds_per_donation or (60 * 5)
end

local function reload_stats_table()
    STATS = util.retrieve_table(STATS_TABLE_NAME) or {}
    STATS.total_time_added = STATS.total_time_added or 0
    STATS.total_time_paused = STATS.total_time_paused or 0
    STATS.total_time_stopped = STATS.total_time_stopped or 0
    STATS.amount_followers = STATS.amount_followers or 0
    STATS.amount_subs_t1 = STATS.amount_subs_t1 or 0
    STATS.amount_subs_t2 = STATS.amount_subs_t2 or 0
    STATS.amount_subs_t3 = STATS.amount_subs_t3 or 0
    STATS.amount_subs_t1_gifted = STATS.amount_subs_t1_gifted or 0
    STATS.amount_subs_t2_gifted = STATS.amount_subs_t2_gifted or 0
    STATS.amount_subs_t3_gifted = STATS.amount_subs_t3_gifted or 0
    STATS.amount_bits = STATS.amount_bits or 0
    STATS.amount_donations = STATS.amount_donations or 0
    STATS.amount_channel_points = STATS.amount_channel_points or 0
    STATS.count_bits = STATS.count_bits or 0
    STATS.count_donations = STATS.count_donations or 0
    STATS.count_channel_points = STATS.count_channel_points or 0
    STATS.redeems = STATS.redeems or {}
    STATS.correction_online_time = STATS.correction_online_time or 0
    STATS.correction_active_time = STATS.correction_active_time or 0
    STATS.correction_remaining_time = STATS.correction_remaining_time or 0
    STATS.correction_subs_t1 = STATS.correction_subs_t1 or 0
    STATS.correction_subs_t2 = STATS.correction_subs_t2 or 0
    STATS.correction_subs_t3 = STATS.correction_subs_t3 or 0
    STATS.correction_bits = STATS.correction_bits or 0
    STATS.correction_donations = STATS.correction_donations or 0
end
reload_settings_table()
reload_stats_table()

local obs = deck:Connector('OBS', 'Obs', { enabled = false })
local twitch = deck:Connector('Twitch', { enabled = false })
local main_window = deck:Connector('Window', 'MainWindow', { width = 600, height = 425 })
local secondary_window = deck:Connector('Window', 'SecondaryWindow', { width = 1200, height = 435, visible = false, exit_on_close = false })
local correction_window = deck:Connector('Window', 'CorrectionWindow', { width = 450, height = 400, visible = false, exit_on_close = false })

--twitch.scopes.follower_data = true
--twitch.scopes.subscriber_data = true
--twitch.scopes.redemption_notifications = true
--twitch.scopes.bits_notifications = true
twitch.scopes.view_chat = true
--twitch.scopes.send_chat = true
--twitch.scopes.send_shoutouts = true
--twitch.scopes.send_announcements = true
--twitch.scopes.automod = true
--twitch.scopes.moderate = true

local color_disabled = deck:Colour 'darkred'
local color_connecting = deck:Colour 'gold'
local color_enabled = deck:Colour 'green'

-- Forward declare some functions
local calc_timers
local render_timers
local render_state

local function set_window_title(win, title)
    win.title = title
    local lbl = widgets.create_label(title)
    lbl.bgcolor = deck:Colour 'indigo'
    lbl.alignment = enum.ALIGN_CENTER
    win.main_widget:add_child(lbl, 0, 0, 1, win.main_widget.cols)
end

local function show_settings_window()
    local grid = secondary_window.main_widget
    grid:clear_children()
    grid:set_grid_size(13, 3)
    grid.homogeneous = false

    set_window_title(secondary_window, 'Twitch Marathon Timer - Settings')

    local function add_setting(row, title, key)
        local lbl = widgets.create_label(title)
        grid:add_child(lbl, row, 0)

        local is_numerical = type(SETTINGS[key]) == 'number'
        local initial_text
        if is_numerical then
            initial_text = tostring(SETTINGS[key])
        elseif not SETTINGS[key] then
            initial_text = ''
        else
            initial_text = SETTINGS[key]
        end

        local input_value = widgets.create_input_field(initial_text)
        input_value.numerical = is_numerical
        input_value.input_length = 32
        input_value.on_blur = function(self)
            local value
            if self.numerical then
                value = tonumber(self.text) or 0
            elseif self.text == '' then
                value = nil
            else
                value = self.text
            end
            if SETTINGS[key] ~= value then
                SETTINGS[key] = value
                util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
                if not STATS.event_started_at then
                    calc_timers()
                    render_timers()
                end
                render_state()
            end
        end
        grid:add_child(input_value, row, 1)

        if not input_value.numerical then
            local btn = widgets.create_button('Paste', function(self)
                local txt = util.clipboard_text()
                input_value:set_text(txt)
                if SETTINGS[key] ~= txt then
                    SETTINGS[key] = txt
                    util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
                end
            end)
            grid:add_child(btn, row, 2)
        end
    end

    add_setting(1, 'OBS password', 'obs_password')
    add_setting(2, 'OBS timer ONLINE', 'obs_timer_online')
    add_setting(3, 'OBS timer ACTIVE', 'obs_timer_active')
    add_setting(4, 'OBS timer COUNTDOWN', 'obs_timer_countdown')
    add_setting(5, 'OBS amount SUBS', 'obs_amount_subs')
    add_setting(6, 'OBS amount BITS', 'obs_amount_bits')
    add_setting(7, 'Start time in seconds', 'event_start_offset')
    add_setting(8, 'Seconds per sub T1', 'seconds_per_sub_t1')
    add_setting(9, 'Seconds per sub T2', 'seconds_per_sub_t2')
    add_setting(10, 'Seconds per sub T3', 'seconds_per_sub_t3')
    add_setting(11, 'Seconds per 100 bits', 'seconds_per_bits')
    add_setting(12, 'Seconds per $1.00 donation', 'seconds_per_donation')

    grid:relayout()
    secondary_window.visible = true
end

local function show_event_log_window(log_name)
    local grid = secondary_window.main_widget
    grid:clear_children()
    grid:set_grid_size(12, 12)
    grid.homogeneous = true

    set_window_title(secondary_window, 'Twitch Marathon Timer - Logs for ' .. log_name)

    local data = util.retrieve_event_log(log_name, 10)

    local defs = {}
    table.insert(defs, { title = 'DateTime', field = 'time', cols = 3 })
    table.insert(defs, { title = 'User', field = 'user_name', cols = 2 })

    if log_name == 'follows' then
    elseif log_name == 'subs' then
        table.insert(defs, { title = 'Tier', field = 'tier', cols = 1 })
        table.insert(defs, { title = 'Gifts', field = 'amount', cols = 1 })
        table.insert(defs, { title = 'Months', field = 'months', cols = 1 })
        table.insert(defs, { title = 'Streak', field = 'streak', cols = 1 })
        --table.insert(defs, { title = 'Message', field = 'message', cols = 3 })
    elseif log_name == 'bits' then
        table.insert(defs, { title = 'Bits', field = 'bits', cols = 1 })
        table.insert(defs, { title = 'Message', field = 'message', cols = 6 })
    elseif log_name == 'redeems' then
        table.insert(defs, { title = 'Points', field = 'points', cols = 1 })
        table.insert(defs, { title = 'Redeem', field = 'title', cols = 2 })
        table.insert(defs, { title = 'Message', field = 'message', cols = 4 })
    elseif log_name == 'donations' then
        table.insert(defs, { title = 'Amount', field = 'amount', cols = 1 })
    end

    local col = 0
    for _, def in ipairs(defs) do
        local lbl = widgets.create_label(def.title)
        grid:add_child(lbl, 1, col, 1, def.cols or 1)
        col = col + (def.cols or 1)
    end

    for row, item in ipairs(data) do
        col = 0
        for _, def in ipairs(defs) do
            local value = item[def.field]
            if value then
                local lbl = widgets.create_label(tostring(item[def.field]))
                grid:add_child(lbl, row + 1, col, 1, def.cols or 1)
            end
            col = col + (def.cols or 1)
        end
    end

    secondary_window.visible = true
end

local function perform_corrections(fields, affect_timers)
    local old_bits = math.floor((STATS.amount_bits + STATS.correction_bits) * SETTINGS.seconds_per_bits / 100.0 + 0.5)
    local old_dono = math.floor((STATS.amount_donations + STATS.correction_donations) * SETTINGS.seconds_per_donation + 0.5)

    local changes = {}
    for key, field in pairs(fields) do
        local value = tonumber(field.text) or 0
        STATS['correction_' .. key] = STATS['correction_' .. key] + value
        field:set_text('0')
        changes[key] = value
    end

    if affect_timers then
        local new_bits = math.floor((STATS.amount_bits + STATS.correction_bits) * SETTINGS.seconds_per_bits / 100.0 + 0.5)
        local new_dono = math.floor((STATS.amount_donations + STATS.correction_donations) * SETTINGS.seconds_per_donation + 0.5)

        STATS.total_time_added = STATS.total_time_added + changes.subs_t1 * SETTINGS.seconds_per_sub_t1
        STATS.total_time_added = STATS.total_time_added + changes.subs_t2 * SETTINGS.seconds_per_sub_t2
        STATS.total_time_added = STATS.total_time_added + changes.subs_t3 * SETTINGS.seconds_per_sub_t3
        STATS.total_time_added = STATS.total_time_added + (new_bits - old_bits)
        STATS.total_time_added = STATS.total_time_added + (new_dono - old_dono)
    end

    util.store_table(STATS_TABLE_NAME, STATS)
    calc_timers()
    render_timers()
    render_state()
end

local function show_correction_window()
    local grid = correction_window.main_widget
    grid:clear_children()
    grid:set_grid_size(12, 2)
    grid.homogeneous = false

    set_window_title(correction_window, 'Twitch Marathon Timer - Corrections')

    local function add_field(desc, row, col)
        local inp = widgets.create_input_field('0')
        inp.numerical = true
        inp.input_length = 8
        grid:add_child(inp, row, col)
        local lbl = widgets.create_label(desc)
        grid:add_child(lbl, row, col + 1)
        return inp
    end

    local fields = {}
    fields.online_time = add_field('Online time (seconds)', 1, 0)
    fields.active_time = add_field('Active time (seconds)', 2, 0)
    fields.remaining_time = add_field('Remaining time (seconds)', 3, 0)
    fields.subs_t1 = add_field('T1 subs', 4, 0)
    fields.subs_t2 = add_field('T2 subs', 5, 0)
    fields.subs_t3 = add_field('T3 subs', 6, 0)
    fields.bits = add_field('Bits', 7, 0)
    fields.donations = add_field('Donation ($)', 8, 0)

    local btn
    btn = widgets.create_button('Adjust WITH changing timers', function() perform_corrections(fields, true) end)
    grid:add_child(btn, -2, 0, 1, 2)
    btn = widgets.create_button('Adjust WITHOUT changing timers', function() perform_corrections(fields, false) end)
    grid:add_child(btn, -1, 0, 1, 2)

    grid:relayout()
    correction_window.visible = true
end

local grid = widgets.create_grid(1, 1)
grid.bgcolor = deck:Colour 'Black'
widgets.connect(grid, secondary_window)

grid = widgets.create_grid(1, 1)
grid.bgcolor = deck:Colour 'Black'
widgets.connect(grid, correction_window)

grid = widgets.create_grid(14, 6)
grid.bgcolor = deck:Colour 'Black'
widgets.connect(grid, main_window)

set_window_title(main_window, 'Twitch Marathon Timer')

local row = 0

local function create_key_value(text)
    row = row + 1
    local key = widgets.create_label(text)
    grid:add_child(key, row, 0, 1, 2)
    local value = widgets.create_label('...')
    value.alignment = enum.ALIGN_CENTER
    grid:add_child(value, row, 2, 1, 2)
    return value, key
end

local lbl_started_at, lbl_started = create_key_value('Started:')
lbl_started.visible = false
lbl_started_at.visible = false

local lbl_paused_at, lbl_paused = create_key_value('Paused:')
lbl_paused.visible = false
lbl_paused_at.visible = false

local lbl_online_time = create_key_value('Online time:')
local start_button = widgets.create_button('Start')
grid:add_child(start_button, row, 4)
local stop_button = widgets.create_button('Stop')
grid:add_child(stop_button, row, 5)

local lbl_active_time = create_key_value('Active time:')
local resume_button = widgets.create_button('Resume')
grid:add_child(resume_button, row, 4)
local pause_button = widgets.create_button('Pause')
grid:add_child(pause_button, row, 5)

local lbl_remaining_time = create_key_value('Time remaining:')

row = row + 1

local lbl_follows = create_key_value('Follows:')
grid:add_child(widgets.create_button('Log', function() show_event_log_window('follows') end), row, 4)
local lbl_subs = create_key_value('Subs:')
grid:add_child(widgets.create_button('Log', function() show_event_log_window('subs') end), row, 4)
local lbl_bits = create_key_value('Bits:')
grid:add_child(widgets.create_button('Log', function() show_event_log_window('bits') end), row, 4)
local lbl_channel_points = create_key_value('Channel points:')
grid:add_child(widgets.create_button('Log', function() show_event_log_window('redeems') end), row, 4)
local lbl_donations = create_key_value('Donations:')
grid:add_child(widgets.create_button('Log', function() show_event_log_window('donations') end), row, 4)


local timestamps = {}
calc_timers = function(total_time)
    if not total_time then
        total_time = timestamps.total or 0
    else
        timestamps.total = total_time
    end

    if STATS.event_stopped_at then
        total_time = os.difftime(STATS.event_stopped_at, STATS.event_started_at)
    end
    timestamps.online = total_time - STATS.total_time_stopped

    if STATS.event_finished_at then
        total_time = os.difftime(STATS.event_finished_at, STATS.event_started_at)
    elseif STATS.event_paused_at then
        total_time = os.difftime(STATS.event_paused_at, STATS.event_started_at)
    end
    timestamps.active = total_time - STATS.total_time_paused
    timestamps.remaining = SETTINGS.event_start_offset + STATS.total_time_added - timestamps.active

    timestamps.online = timestamps.online + STATS.correction_online_time
    timestamps.active = timestamps.active + STATS.correction_active_time
    timestamps.remaining = timestamps.remaining + STATS.correction_remaining_time

    if timestamps.remaining < 0 then
        timestamps.remaining = 0
    end

    return timestamps
end

local function format_time(seconds)
    local hours = math.floor(seconds / 3600)
    seconds = seconds - hours * 3600
    local minutes = math.floor(seconds / 60)
    seconds = seconds - minutes * 60
    return string.format('%d:%02d:%02d', hours, minutes, seconds)
end

render_timers = function()
    if not timestamps then
        return
    end
    local online_time = format_time(timestamps.online)
    local active_time = format_time(timestamps.active)
    local remaining_time = format_time(timestamps.remaining)

    lbl_online_time:set_text(online_time)
    lbl_active_time:set_text(active_time)
    lbl_remaining_time:set_text(remaining_time)

    if obs.connected then
        if SETTINGS.obs_timer_online then
            obs:SetInputSettings(SETTINGS.obs_timer_online, { text = online_time })
        end
        if SETTINGS.obs_timer_active then
            obs:SetInputSettings(SETTINGS.obs_timer_active, { text = active_time })
        end
        if SETTINGS.obs_timer_countdown then
            obs:SetInputSettings(SETTINGS.obs_timer_countdown, { text = remaining_time })
        end
    end
end

render_state = function()
    local txt

    if STATS.event_started_at then
        lbl_started:set_visible(true)
        lbl_started_at:set_text(os.date('%d %b, %T', STATS.event_started_at))
        lbl_started_at:set_visible(true)
    else
        lbl_started:set_visible(false)
        lbl_started_at:set_visible(false)
    end

    if STATS.event_finished_at then
        lbl_paused:set_text('Finished: ')
        lbl_paused:set_visible(true)
        lbl_paused_at:set_text(os.date('%d %b, %T', STATS.event_finished_at))
        lbl_paused_at:set_visible(true)
    elseif STATS.event_stopped_at then
        lbl_paused:set_text('Stopped: ')
        lbl_paused:set_visible(true)
        lbl_paused_at:set_text(os.date('%d %b, %T', STATS.event_stopped_at))
        lbl_paused_at:set_visible(true)
    elseif STATS.event_paused_at then
        lbl_paused:set_text('Paused: ')
        lbl_paused:set_visible(true)
        lbl_paused_at:set_text(os.date('%d %b, %T', STATS.event_paused_at))
        lbl_paused_at:set_visible(true)
    else
        lbl_paused:set_visible(false)
        lbl_paused_at:set_visible(false)
    end

    start_button:set_enabled(not STATS.event_started_at or STATS.event_stopped_at)
    stop_button:set_enabled(STATS.event_started_at and not STATS.event_stopped_at)
    pause_button:set_enabled(not STATS.event_finished_at and STATS.event_started_at and not STATS.event_paused_at and not STATS.event_stopped_at)
    resume_button:set_enabled(not STATS.event_finished_at and STATS.event_started_at and (STATS.event_paused_at or STATS.event_stopped_at))

    lbl_follows:set_text(tostring(STATS.amount_followers))

    txt = string.format('%d / %d / %d',
        STATS.amount_subs_t1 + STATS.amount_subs_t1_gifted + STATS.correction_subs_t1,
        STATS.amount_subs_t2 + STATS.amount_subs_t2_gifted + STATS.correction_subs_t2,
        STATS.amount_subs_t3 + STATS.amount_subs_t3_gifted + STATS.correction_subs_t3)
    lbl_subs:set_text(txt)

    lbl_bits:set_text(tostring(STATS.amount_bits + STATS.correction_bits))

    lbl_channel_points:set_text(tostring(STATS.amount_channel_points))

    local money = STATS.amount_donations + STATS.correction_donations + 0.005
    local eddies = math.floor(money)
    local cents = math.floor((money - eddies) * 100)
    local txt = string.format('$ %d.%02d', eddies, cents)
    lbl_donations:set_text(txt)

    if obs.connected then
        if SETTINGS.obs_amount_subs then
            local total_subs = STATS.amount_subs_t1 + STATS.amount_subs_t2 + STATS.amount_subs_t3
            local total_subs = total_subs + STATS.amount_subs_t1_gifted + STATS.amount_subs_t2_gifted + STATS.amount_subs_t3_gifted
            local total_subs = total_subs + STATS.correction_subs_t1 + STATS.correction_subs_t2 + STATS.correction_subs_t3
            obs:SetInputSettings(SETTINGS.obs_amount_subs, { text = tostring(total_subs) })
        end
        if SETTINGS.obs_amount_bits then
            obs:SetInputSettings(SETTINGS.obs_amount_bits, { text = tostring(STATS.amount_bits + STATS.correction_bits) })
        end
    end
end

start_button.callback = function(self)
    local now = os.time()

    if not STATS.event_started_at then
        STATS.event_started_at = now
    elseif STATS.event_stopped_at then
        local diff = os.difftime(now, STATS.event_stopped_at)
        STATS.total_time_stopped = STATS.total_time_stopped + diff
        if not STATS.event_finished_at then
            STATS.total_time_paused = STATS.total_time_paused + diff
        end
        STATS.event_stopped_at = nil
        STATS.event_paused_at = now
    end
    util.store_table(STATS_TABLE_NAME, STATS)

    render_state()
end

stop_button.callback = function(self)
    local now = os.time()

    STATS.event_stopped_at = now
    if STATS.event_paused_at then
        STATS.total_time_paused = STATS.total_time_paused + os.difftime(now, STATS.event_paused_at)
        STATS.event_paused_at = nil
    end

    util.store_table(STATS_TABLE_NAME, STATS)

    render_state()
end

resume_button.callback = function(self)
    local now = os.time()

    if STATS.event_stopped_at then
        local diff = os.difftime(now, STATS.event_stopped_at)
        STATS.total_time_stopped = STATS.total_time_stopped + diff
        STATS.total_time_paused = STATS.total_time_paused + diff
    elseif STATS.event_paused_at then
        STATS.total_time_paused = STATS.total_time_paused + os.difftime(now, STATS.event_paused_at)
    end
    STATS.event_paused_at = nil
    STATS.event_stopped_at = nil
    util.store_table(STATS_TABLE_NAME, STATS)

    render_state()
end

pause_button.callback = function(self)
    STATS.event_paused_at = os.time()
    util.store_table(STATS_TABLE_NAME, STATS)

    render_state()
end


local reload_tables_button = widgets.create_button('Reload', function(self)
    reload_settings_table()
    reload_stats_table()
    render_state()
    if not STATS.event_started_at then
        calc_timers()
        render_timers()
    end
end)
grid:add_child(reload_tables_button, -1, 0)

local correct_button = widgets.create_button('Correct', function(self)
    show_correction_window()
end)
grid:add_child(correct_button, -1, 1)

local obs_button = widgets.create_button('OBS', function(self)
    if not SETTINGS.obs_password then
        show_settings_window()
    elseif not obs.enabled then
        obs.password = SETTINGS.obs_password
        obs.enabled = true
        self.bgcolor = color_connecting
        self:redraw(true)
    elseif not obs.connected then
        obs.enabled = false
        self.bgcolor = color_disabled
        self:redraw(true)
    else
        obs.enabled = false
        self.bgcolor = color_connecting
        self:redraw(true)
    end
end)
if obs.connected then
    obs_button.bgcolor = color_enabled
elseif obs.enabled then
    obs_button.bgcolor = color_connecting
else
    obs_button.bgcolor = color_disabled
end
grid:add_child(obs_button, -1, -2)

obs.on_connect_failed = function(self, reason)
    logger(logger.WARNING, 'OBS connection failed: ' .. reason)
    self.connected = false
end
obs.on_connect = function(self)
    logger(logger.INFO, 'OBS connected!')
    obs_button.bgcolor = color_enabled
    obs_button:redraw(true)
    self.connected = true
    render_timers()
    render_state()
end
obs.on_disconnect = function(self)
    logger(logger.INFO, 'OBS disconnected!')
    obs_button.bgcolor = color_disabled
    obs_button:redraw(true)
    self.connected = false
end

local twitch_button = widgets.create_button('Twitch', function(self)
    twitch.enabled = not twitch.enabled
end)
if twitch._internal_state == twitch.INACTIVE then
    twitch_button.bgcolor = color_disabled
elseif twitch._internal_state == twitch.ACTIVE then
    twitch_button.bgcolor = color_enabled
else
    twitch_button.bgcolor = color_connecting
end
grid:add_child(twitch_button, -1, -1)

twitch.on_state_changed = function(self, newstate, msg)
    logger(logger.INFO, 'Twitch:', msg)
    if newstate == self.INACTIVE then
        twitch_button.bgcolor = color_disabled
    elseif newstate == self.ACTIVE then
        twitch_button.bgcolor = color_enabled
    else
        twitch_button.bgcolor = color_connecting
    end
    twitch_button:redraw(true)
end

local function add_time(seconds)
    if seconds and not STATS.event_finished_at then
        STATS.total_time_added = STATS.total_time_added + seconds
        calc_timers()
        render_timers()
    end
end

local function tier_to_key(tier)
    local postfix
    if tier == '2000' then
        postfix = 't2'
    elseif tier == '3000' then
        postfix = 't3'
    else
        postfix = 't1'
    end
    return 'amount_subs_' .. postfix, 'seconds_per_sub_' .. postfix
end

twitch.on_follow = function(self, channel, user)
    print('FOLLOW', user.name)

    STATS.amount_followers = STATS.amount_followers + 1
    util.store_table(STATS_TABLE_NAME, STATS)
    render_state()

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name }
    util.append_event_log('follows', log)
end
twitch.on_subscribe = function(self, channel, user, tier, is_gift)
    print('SUB', user.name, 'TIER', tier, 'GIFT', is_gift)

    if is_gift then
        local ts = os.time()
        local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name, tier = tier }
        util.append_event_log('gifted_subs', log)
    else
        --[[
        There is a problem with the Twitch API in that new subs sometimes send TWO messages,
        but sometimes don't. A new sub can either sub and not send a message (choose to not display "subbed for X months"),
        or the user can choose to send that message. This other message comes in through the RESUB notification.
        When this SUB message comes in, we simply cannot know whether the other message will come or not.
        Only Twitch knows.

        In this script, we opt for not counting this message, as we feel that only very few people
        opt to not send a text message with their subscription. This means the user of this script needs to put in
        a positive correction for every person that does not enter a message, as opposed to needing to put in
        a negative correction for every person that does enter a message.
        --]]

        --local stat_key, setting_key = tier_to_key(tier)
        --STATS[stat_key] = STATS[stat_key] + 1
        --add_time(SETTINGS[setting_key])
        --util.store_table(STATS_TABLE_NAME, STATS)
        --render_state()

        local ts = os.time()
        local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'sub', user_name = user.name, tier = tier }
        util.append_event_log('subs', log)
    end
end
twitch.on_unsubscribe = function(self, channel, user, tier, is_gift)
    print('UNSUB', user.name, 'TIER', tier, 'GIFT', is_gift)

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name, tier = tier, was_gift = is_gift }
    util.append_event_log('expired_subs', log)
end
twitch.on_subscription_gift = function(self, channel, user, tier, total, cumulative_total)
    print('GIFT', user.name, 'TIER', tier, 'AMOUNT', total, 'TOTAL', cumulative_total)

    local stat_key, setting_key = tier_to_key(tier)
    stat_key = stat_key .. '_gifted'
    STATS[stat_key] = STATS[stat_key] + total
    local time_added = SETTINGS[setting_key] * total
    add_time(time_added)
    util.store_table(STATS_TABLE_NAME, STATS)
    render_state()

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'gift', user_name = user.name or '(anonymous)', tier = tier, amount = total, cumulative_total = cumulative_total, time_added = time_added }
    util.append_event_log('subs', log)
end
twitch.on_resubscribe = function(self, channel, user, tier, months, streak, message)
    print('RESUB', user.name, 'TIER', tier, 'MONTHS', months, 'STREAK', streak, 'MESSAGE', message)

    local stat_key, setting_key = tier_to_key(tier)
    STATS[stat_key] = STATS[stat_key] + 1
    add_time(SETTINGS[setting_key])
    util.store_table(STATS_TABLE_NAME, STATS)
    render_state()

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'resub', user_name = user.name, tier = tier, months = months, streak = streak, message = message, time_added = SETTINGS[setting_key] }
    util.append_event_log('subs', log)
end
twitch.on_cheer = function(self, channel, user, bits, message)
    print('CHEER', user.name, 'BITS', bits, 'MESSAGE', message)

    local prev_added = math.floor((STATS.amount_bits + STATS.correction_bits) * SETTINGS.seconds_per_bits / 100.0 + 0.5)
    STATS.amount_bits = STATS.amount_bits + bits
    STATS.count_bits = STATS.count_bits + 1
    local new_added = math.floor((STATS.amount_bits + STATS.correction_bits) * SETTINGS.seconds_per_bits / 100.0 + 0.5)
    add_time(new_added - prev_added)
    util.store_table(STATS_TABLE_NAME, STATS)
    render_state()

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name, bits = bits, message = message, time_added = new_added - prev_added }
    util.append_event_log('bits', log)
end
twitch.on_channel_points = function(self, channel, user, points, title, message)
    print('REDEEM', user.name, 'POINTS', points, 'TITLE', title, 'MESSAGE', message)

    STATS.amount_channel_points = STATS.amount_channel_points + points
    STATS.count_channel_points = STATS.count_channel_points + 1
    STATS.redeems[title] = (STATS.redeems[title] or 0) + 1
    util.store_table(STATS_TABLE_NAME, STATS)
    render_state()

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name or '(anonymous)', points = points, title = title, message = message }
    util.append_event_log('redeems', log)
end
twitch.on_channel_chat_clear = function(self, channel)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'clear_chat', channel = channel }
    util.append_event_log('chat', log)
end
twitch.on_channel_chat_message = function(self, channel, message_id, user, message, extra)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'message', channel = channel, user_name = user.name, message = message, extra = extra }
    util.append_event_log('chat', log)
end
twitch.on_channel_chat_message_delete = function(self, channel, message_id, user)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), channel = channel, user_name = user.name }
    if message_id then
        log.message_id = message_id
        log.event = 'delete_message'
    else
        log.event = 'clear_user'
    end
    util.append_event_log('chat', log)
end
twitch.on_channel_chat_notification = function(self, channel, event)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), channel = channel, event = 'notification', notification = event }
    util.append_event_log('chat', log)
end

local settings_button = widgets.create_button('Settings', function(self)
    show_settings_window()
end)
grid:add_child(settings_button, 1, -1)


render_state()

local last_total_time = 0
if not STATS.event_started_at then
    calc_timers(0)
    render_timers()
end

local function tick(clock)
    if STATS.event_started_at then
        local now = os.time()
        local total_time = os.difftime(now, STATS.event_started_at)
        if total_time ~= last_total_time then
            last_total_time = total_time
            calc_timers(total_time)
            render_timers()
            if timestamps.remaining == 0 and not STATS.event_finished_at then
                STATS.event_finished_at = now
                util.store_table(STATS_TABLE_NAME, STATS)
                render_state()
            end
        end
    end
end

return {
    tick = tick,
    connectors = {
        obs,
        twitch,
        main_window,
        secondary_window,
        correction_window
    }
}
