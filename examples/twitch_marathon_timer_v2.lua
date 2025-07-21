local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')
local widgets = require('deck.widgets')
local enum = require('deck.enum')
require('connector.obs')
require('connector.twitch')

widgets.default_font.size = 20
local underline_font = widgets.default_font:clone { enum.STYLE_UNDERLINE }

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


-- Constants
local color_disabled = deck:Colour 'darkred'
local color_connecting = deck:Colour 'gold'
local color_enabled = deck:Colour 'green'
local color_active = deck:Colour 'white'
local color_dimmed = deck:Colour '#808080'

local k_mode_unstarted = 'UNSTARTED'
local k_mode_active = 'ACTIVE'
local k_mode_inactive = 'INACTIVE'
local k_mode_resting = 'RESTING'
local k_mode_frozen = 'FROZEN'
local k_mode_capped = 'CAPPED'
local k_mode_finished = 'FINISHED'

local SETTINGS_TABLE_NAME = 'settings'
local STATS_TABLE_NAME = 'stats'


-- Forward declarations of functions
local swap_mode
local update_total_daily_stats

-- Forward declarations for main window items
local lbl_started_at
local lbl_time_mode_at_key
local lbl_time_mode_at
local lbl_time_in_mode_key
local lbl_time_in_mode
local btn_switch_mode
local btn_corrections
local btn_points_log
local btn_connect_obs
local btn_connect_twitch
local lbl_time_total
local lbl_time_remaining
local lbl_time_resting
local lbl_time_active
local lbl_time_inactive
local lbl_time_buffered
local lbl_time_rested
local lbl_time_frozen
local lbl_points_total, lbl_points_today
local lbl_ratio_points_today
local lbl_subs_total, lbl_subs_today
local lbl_bits_total, lbl_bits_today
local lbl_donations_total, lbl_donations_today
local lbl_followers_total, lbl_followers_today
local lbl_redeems_total, lbl_redeems_today

local OBS_LABELS = {}

-- Settings
local function reload_settings_table()
    local settings = util.retrieve_table(SETTINGS_TABLE_NAME) or {}
    settings.event_start_offset = settings.event_start_offset or 3600
    settings.max_rest_timer = settings.max_rest_timer or (4 * 3600)
    settings.added_seconds_per_point = settings.added_seconds_per_point or 1800
    settings.points_per_sub_t1 = settings.points_per_sub_t1 or 1
    settings.points_per_sub_t2 = settings.points_per_sub_t2 or 2
    settings.points_per_sub_t3 = settings.points_per_sub_t3 or 5
    settings.bits_per_point = settings.bits_per_point or 500
    settings.cents_per_point = settings.cents_per_point or 500
    settings.rest_bar_spout_name = settings.rest_bar_spout_name or 'Deck-RestBar'
    settings.rest_bar_width = settings.rest_bar_width or 50
    settings.rest_bar_height = settings.rest_bar_height or 800
    settings.rest_bar_border_size = settings.rest_bar_border_size or 2
    settings.rest_bar_border_color = settings.rest_bar_border_color or 'black'
    settings.rest_bar_bgcolor = settings.rest_bar_bgcolor or '#80808020'
    settings.rest_bar_fgcolor = settings.rest_bar_fgcolor or 'blue'
    settings.ratio_bar_spout_name = settings.ratio_bar_spout_name or 'Deck-RatioBar'
    settings.ratio_bar_width = settings.ratio_bar_width or 400
    settings.ratio_bar_height = settings.ratio_bar_height or 20
    settings.ratio_bar_border_size = settings.ratio_bar_border_size or 2
    settings.ratio_bar_border_color = settings.ratio_bar_border_color or 'black'
    settings.ratio_bar_empty_color = settings.ratio_bar_empty_color or 'white'
    settings.ratio_bar_subs_color = settings.ratio_bar_subs_color or '#ff0000'
    settings.ratio_bar_bits_color = settings.ratio_bar_bits_color or '#00ff00'
    settings.ratio_bar_dono_color = settings.ratio_bar_dono_color or '#0000ff'
    return settings
end

local function reload_stats_table()
    local stats = util.retrieve_table(STATS_TABLE_NAME) or {}
    if stats.date_started then
        stats.date_mode_started = stats.date_mode_started or stats.date_started
    end
    stats.time_buffered = stats.time_buffered or 0

    stats.time_total_inactive = stats.time_total_inactive or 0
    stats.time_total_rested = stats.time_total_rested or 0
    stats.time_total_frozen = stats.time_total_frozen or 0

    stats.time_added = stats.time_added or 0
    stats.time_correction = stats.time_correction or 0
    stats.time_rest_overflowed = stats.time_rest_overflowed or 0

    local function subtable(tag)
        stats[tag] = stats[tag] or {}
        local sub = stats[tag]
        sub.amount_followers = sub.amount_followers or 0
        sub.amount_subs_t1 = sub.amount_subs_t1 or 0
        sub.amount_subs_t2 = sub.amount_subs_t2 or 0
        sub.amount_subs_t3 = sub.amount_subs_t3 or 0
        sub.amount_subs_t1_gifted = sub.amount_subs_t1_gifted or 0
        sub.amount_subs_t2_gifted = sub.amount_subs_t2_gifted or 0
        sub.amount_subs_t3_gifted = sub.amount_subs_t3_gifted or 0
        sub.amount_bits = sub.amount_bits or 0
        sub.amount_donations = sub.amount_donations or 0
        sub.amount_channel_points = sub.amount_channel_points or 0
        sub.count_bits = sub.count_bits or 0
        sub.count_donations = sub.count_donations or 0
        sub.count_channel_points = sub.count_channel_points or 0
        sub.redeems = sub.redeems or {}
    end
    subtable('total')
    subtable('today')
    return stats
end
local SETTINGS = reload_settings_table()
local STATS = reload_stats_table()

-- Custom widgets
local rest_bar_widget = widgets.default_widget()
rest_bar_widget.resize = function(self, width, height)
    if not self.card or self.card.width ~= width or self.card.height ~= height then
        self.card = deck:Card(width, height)
        self:redraw()
    end
end
rest_bar_widget.redraw = function(self)
    if self.card and self._time_value then
        local black = deck:Colour('#000000')
        local bgcolor = deck:Colour(SETTINGS.rest_bar_bgcolor)
        local fgcolor = deck:Colour(SETTINGS.rest_bar_fgcolor)

        local buffered = self._time_value
        local space = self._max_value - buffered

        self.card:clear(black)
        if buffered <= 0 then
            self.card:clear(bgcolor)
        elseif space <= 0 then
            self.card:clear(fgcolor)
        else
            local separator = math.floor(self.card.height * space / self._max_value)
            local empty = deck:Rectangle(self.card.width, separator)
            local filled = deck:Rectangle(0, separator, self.card.width, self.card.height - separator)
            self.card:sub_card(empty):clear(bgcolor)
            self.card:sub_card(filled):clear(fgcolor)
        end
        if self.on_update then
            self:on_update()
        end
    end
end
rest_bar_widget.set_time = function(self, time_value)
    if self._time_value ~= time_value or self._max_value ~= SETTINGS.max_rest_timer then
        self._time_value = time_value
        self._max_value = SETTINGS.max_rest_timer
        self:redraw()
    end
end

local ratio_bar_widget = widgets.default_widget()
ratio_bar_widget.resize = function(self, width, height)
    if not self.card or self.card.width ~= width or self.card.height ~= height then
        self.card = deck:Card(width, height)
        self:redraw()
    end
end
ratio_bar_widget.redraw = function(self)
    if self.card then
        local black = deck:Colour('#000000')
        local empty_color = deck:Colour(SETTINGS.ratio_bar_empty_color)
        local subs_color = deck:Colour(SETTINGS.ratio_bar_subs_color)
        local dono_color = deck:Colour(SETTINGS.ratio_bar_bits_color)
        local bits_color = deck:Colour(SETTINGS.ratio_bar_dono_color)

        self.card:clear(black)
        self.card:clear(empty_color)

        if self.on_update then
            self:on_update()
        end
    end
end

local function format_date(timestamp)
    return os.date('%b %e, %H:%M:%S', timestamp)
end

local function format_time(seconds, allow_negative)
    local is_negative = allow_negative and (seconds < 0)
    if seconds < 0 then
        seconds = is_negative and -seconds or 0
    end
    local hours = math.floor(seconds / 3600)
    seconds = seconds - hours * 3600
    local minutes = math.floor(seconds / 60)
    seconds = seconds - minutes * 60
    return string.format('%s%d:%02d:%02d', is_negative and '-' or '', hours, minutes, seconds)
end

local function format_float(value)
    local s = string.format('%.04f', value)
    local len = string.len(s)
    while len > 1 do
        local c = string.sub(s, len)
        if c == '0' or c == '.' then
            len = len - 1
            s = string.sub(s, 0, len)
        end
        if c ~= '0' then
            break
        end
    end
    return s
end

local function save_setting(key, value)
    local existing_value = SETTINGS[key]
    if value == existing_value then
        return
    end
    assert(existing_value == nil or value == nil or type(existing_value) == type(value), 'Type mismatch while storing setting ' .. key)
    SETTINGS[key] = value
    util.store_table(SETTINGS_TABLE_NAME, SETTINGS)
end

local function save_stats()
    util.store_table(STATS_TABLE_NAME, STATS)
    update_total_daily_stats()
    rest_bar_widget:redraw()
    ratio_bar_widget:redraw()
end

local function add_stats(items)
    local function add_stats_recursive(target, things)
        for key, value in pairs(things) do
            if type(value) == 'number' then
                target[key] = (target[key] or 0) + value
            elseif type(value) == 'table' then
                target[key] = target[key] or {}
                add_stats_recursive(target[key], value)
            end
        end
    end
    add_stats_recursive(STATS.total, items)
    add_stats_recursive(STATS.today, items)
end

local function add_points(ts, username, message, points, seconds, grassy)
    if STATS.mode == k_mode_capped or STATS.mode == k_mode_finished then
        seconds = 0
    elseif grassy then
        STATS.time_buffered = (STATS.time_buffered or 0) + seconds
    else
        STATS.time_added = (STATS.time_added or 0) + seconds
    end

    local log = { timestamp = ts, time = os.date('%F %T', ts), user = username, message = message, points = points, seconds = seconds, grass = grassy }
    util.append_event_log('points', log)
end

local function add_bits(ts, username, amount, grassy)
    if amount == 0 then
        return
    end

    add_stats({ amount_bits = amount, count_bits = amount > 0 and 1 or -1 })

    local old_total_bits = STATS.total.amount_bits
    local new_total_bits = old_total_bits + amount
    local old_points_added = old_total_bits / SETTINGS.bits_per_point
    local new_points_added = new_total_bits / SETTINGS.bits_per_point
    local old_time_added = math.floor(old_points_added * SETTINGS.added_seconds_per_point + 0.5)
    local new_time_added = math.floor(new_points_added * SETTINGS.added_seconds_per_point + 0.5)
    add_points(ts, username, string.format('BITS:%d', amount), new_points_added - old_points_added, new_time_added - old_time_added, grassy)
end

local function add_subs(ts, username, amount, tier, is_gift, grassy)
    if amount == 0 then
        return
    end

    local handle
    if tier == 3 then
        handle = 't3'
    elseif tier == 2 then
        handle = 't2'
    else
        handle = 't1'
    end

    local setting_name = 'points_per_sub_' .. handle
    local stat_name = 'amount_subs_' .. handle
    if is_gift then
        stat_name = stat_name .. '_gifted'
    end

    local stats = {}
    stats[stat_name] = amount
    add_stats(stats)

    local points_added = SETTINGS[setting_name] * amount
    local time_added = points_added * SETTINGS.added_seconds_per_point
    add_points(ts, username, string.format('SUBS:T%d:%d', tier, amount), points_added, time_added, grassy)
end

local function reset_daily_stats()
    local tbl = STATS.today
    for key, value in pairs(tbl) do
        if type(value) == 'table' then
            tbl[key] = {}
        elseif type(value) == 'number' then
            tbl[key] = 0
        end
    end
    save_stats()
end

update_total_daily_stats = function()
    local function calc_points(tbl)
        return
            (STATS[tbl].amount_subs_t1 + STATS[tbl].amount_subs_t1_gifted) * SETTINGS.points_per_sub_t1
            + (STATS[tbl].amount_subs_t2 + STATS[tbl].amount_subs_t2_gifted) * SETTINGS.points_per_sub_t2
            + (STATS[tbl].amount_subs_t3 + STATS[tbl].amount_subs_t3_gifted) * SETTINGS.points_per_sub_t3
            , (STATS[tbl].amount_bits / SETTINGS.bits_per_point)
            , (STATS[tbl].amount_donations / SETTINGS.cents_per_point)
    end
    local total_subs, total_bits, total_dono = calc_points('total')
    local today_subs, today_bits, today_dono = calc_points('today')
    local points_total = total_subs + total_bits + total_dono
    local points_today = today_subs + today_bits + today_dono
    local subs_total = string.format('%d / %d / %d',
        STATS.total.amount_subs_t1 + STATS.total.amount_subs_t1_gifted,
        STATS.total.amount_subs_t2 + STATS.total.amount_subs_t2_gifted,
        STATS.total.amount_subs_t3 + STATS.total.amount_subs_t3_gifted)
    local subs_today = string.format('%d / %d / %d',
        STATS.today.amount_subs_t1 + STATS.today.amount_subs_t1_gifted,
        STATS.today.amount_subs_t2 + STATS.today.amount_subs_t2_gifted,
        STATS.today.amount_subs_t3 + STATS.today.amount_subs_t3_gifted)

    -- Do some fancy rounding to build the ratio string
    local ratio_bits = points_today > 0 and math.ceil(100 * today_bits / points_today) or 0
    local ratio_dono = points_today > 0 and math.ceil(100 * today_dono / points_today) or 0
    if ratio_bits + ratio_dono > 100 then
        if ratio_bits > ratio_dono then
            ratio_bits = 100 - ratio_dono
        else
            ratio_dono = 100 - ratio_bits
        end
    end
    local ratio_subs = points_today > 0 and (100 - ratio_bits - ratio_dono) or 0
    local ratio_today = string.format('S:%d%%  B:%d%%  D:%d%%', ratio_subs, ratio_bits, ratio_dono)

    local donos_total_units = math.floor(STATS.total.amount_donations / 100)
    local donos_total_cents = STATS.total.amount_donations - (donos_total_units * 100)
    local donos_today_units = math.floor(STATS.today.amount_donations / 100)
    local donos_today_cents = STATS.today.amount_donations - (donos_today_units * 100)
    local donos_total = string.format('$ %d.%02d', donos_total_units, donos_total_cents)
    local donos_today = string.format('$ %d.%02d', donos_today_units, donos_today_cents)

    lbl_points_total:set_text(format_float(points_total))
    lbl_points_today:set_text(format_float(points_today))
    lbl_subs_total:set_text(subs_total)
    lbl_subs_today:set_text(subs_today .. string.format(' (%d%%)', ratio_subs))
    lbl_bits_total:set_text(tostring(STATS.total.amount_bits))
    lbl_bits_today:set_text(tostring(STATS.today.amount_bits) .. string.format(' (%d%%)', ratio_bits))
    lbl_donations_total:set_text(donos_total)
    lbl_donations_today:set_text(donos_today .. string.format(' (%d%%)', ratio_dono))
    lbl_followers_total:set_text(tostring(STATS.total.amount_followers))
    lbl_followers_today:set_text(tostring(STATS.today.amount_followers))
    lbl_redeems_total:set_text(tostring(STATS.total.amount_channel_points))
    lbl_redeems_today:set_text(tostring(STATS.today.amount_channel_points))
    lbl_ratio_points_today:set_text(ratio_today)
end

-- Connector setup
local obs = deck:Connector('OBS', 'Obs', { enabled = false, password = SETTINGS.obs_password })
local twitch = deck:Connector('Twitch', { enabled = false })
local main_window = deck:Connector('Window', 'MainWindow', { width = 600, height = 580 })
local secondary_window = deck:Connector('Window', 'SecondaryWindow', { width = 840, height = 550, visible = false, exit_on_close = false })

-- Spout connectors
local _, rest_bar = pcall(deck.Connector, deck.Connector, 'Spout', 'RestBar', {})
if not _ then
    rest_bar = deck:Connector('Window', 'RestBarWindow', { exit_on_close = false })
end
rest_bar.width = SETTINGS.rest_bar_width
rest_bar.height = SETTINGS.rest_bar_height
widgets.connect(rest_bar_widget, rest_bar)

local _, ratio_bar = pcall(deck.Connector, deck.Connector, 'Spout', 'RatioBar', {})
if not _ then
    ratio_bar = deck:Connector('Window', 'RatioBarWindow', { exit_on_close = false })
end
ratio_bar.width = SETTINGS.ratio_bar_width
ratio_bar.height = SETTINGS.ratio_bar_height
widgets.connect(ratio_bar_widget, ratio_bar)


twitch.scopes.follower_data = true
twitch.scopes.redemption_notifications = true
twitch.scopes.bits_notifications = true
twitch.scopes.view_chat = true

local main_wm = widgets.create_window_manager()
widgets.connect(main_wm, main_window)

local main_grid = widgets.create_grid(1, 1)
main_grid.expand = true
main_wm:add_child(main_grid)
main_window.grid = main_grid

local secondary_grid = widgets.create_grid(1, 1)
secondary_grid.bgcolor = deck:Colour 'Black'
widgets.connect(secondary_grid, secondary_window)
secondary_window.grid = secondary_grid

local function subtier(twitch_tier)
    local i = tonumber(twitch_tier, 10) or 0
    return math.max(math.floor(i / 1000), 1)
end

local function set_grid_title(grid, title)
    local lbl = widgets.create_label(title)
    lbl.bgcolor = deck:Colour 'indigo'
    lbl.alignment = enum.ALIGN_CENTER
    grid:add_child(lbl, 0, 0, 1, grid.cols)
end

local function set_window_title(win, title)
    win.title = title
    set_grid_title(win.grid, title)
end

local function show_settings_window()
    local grid = secondary_grid
    grid:clear_children()
    grid:set_grid_size(19, 3)
    grid.homogeneous = false

    secondary_window.height = 580
    secondary_window.width = 840

    set_window_title(secondary_window, 'Twitch Marathon Timer - Settings')

    local row = 0
    local function add_setting(title, key)
        row = row + 1

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
            save_setting(key, value)
        end
        grid:add_child(input_value, row, 1)

        if not input_value.numerical then
            local btn = widgets.create_button('Paste', function(self)
                local txt = util.clipboard_text()
                input_value:set_text(txt)
                save_setting(key, txt)
            end)
            grid:add_child(btn, row, 2)
        end
    end

    add_setting('OBS password', 'obs_password')
    add_setting('OBS timer TOTAL', 'obs_timer_total')
    add_setting('OBS timer ACTIVE', 'obs_timer_active')
    add_setting('OBS timer REMAINING', 'obs_timer_remaining')
    add_setting('OBS timer REST BUFFER', 'obs_timer_rest_buffer')
    add_setting('OBS timer REST ACTIVE', 'obs_timer_rest_active')
    add_setting('OBS timer CURRENT MODE', 'obs_timer_current_mode')
    add_setting('OBS amount POINTS TOTAL', 'obs_amount_points_total')
    add_setting('OBS amount POINTS TODAY', 'obs_amount_points_today')
    add_setting('OBS ratio POINTS TODAY', 'obs_ratio_points_today')
    add_setting('Max Rest Timer in seconds', 'max_rest_timer')
    add_setting('Start time in seconds', 'event_start_offset')
    add_setting('Added seconds per point', 'added_seconds_per_point')
    add_setting('Points per sub T1', 'points_per_sub_t1')
    add_setting('Points per sub T2', 'points_per_sub_t2')
    add_setting('Points per sub T3', 'points_per_sub_t3')
    add_setting('Bits per point', 'bits_per_point')
    add_setting('Cents per point', 'cents_per_point')

    grid:relayout()
    secondary_window.visible = true
end

local function show_switch_mode_screen(btn)
    local grid = widgets.create_grid(10, 5)
    grid.bgcolor = deck:Colour 'Black'
    grid.margin = 0
    grid.expand = false

    set_grid_title(grid, 'Switch MODE')

    local row = 1

    row = row + 1
    local btn_go_active = widgets.create_button('Active')
    btn_go_active.enabled = STATS.mode ~= k_mode_active
    grid:add_child(btn_go_active, row, 1, 1, 3)

    row = row + 1
    local btn_go_inactive = widgets.create_button('Sleep')
    btn_go_inactive.enabled = STATS.mode ~= k_mode_inactive
    grid:add_child(btn_go_inactive, row, 1, 1, 3)

    row = row + 1
    local time_buffered = STATS.time_buffered
    if time_buffered > SETTINGS.max_rest_timer then
        time_buffered = SETTINGS.max_rest_timer
    end
    local btn_go_touch_grass = widgets.create_button('Touch Grass (' .. format_time(time_buffered) .. ')')
    btn_go_touch_grass.enabled = STATS.mode ~= k_mode_resting and time_buffered > 0
    grid:add_child(btn_go_touch_grass, row, 1, 1, 3)

    row = row + 1
    local btn_go_freeze = widgets.create_button('Freeze')
    btn_go_freeze.enabled = STATS.mode ~= k_mode_frozen
    grid:add_child(btn_go_freeze, row, 1, 1, 3)

    row = row + 2
    local btn_reset_dailies = widgets.create_button('Reset DAILY counters')
    grid:add_child(btn_reset_dailies, row, 1, 1, 3)

    local btn_cancel = widgets.create_button('Cancel')
    grid:add_child(btn_cancel, -1, 0, 1, grid.cols)

    local bg_gray = deck:Colour { r = 20, g = 20, b = 20, a = 180 }
    local bg = widgets.create_color_rect(bg_gray)
    main_wm:add_child(bg)

    local border = widgets.create_border()
    border:add_child(grid)
    border.expand = false
    border.size = 4
    border.margin = 0
    border.min_width = 420
    main_wm:add_child(border)

    local remove_window = function()
        main_wm:remove_child(bg)
        main_wm:remove_child(border)
    end

    btn_go_active.callback = function()
        swap_mode(k_mode_active)
        remove_window()
    end
    btn_go_inactive.callback = function()
        swap_mode(k_mode_inactive)
        remove_window()
    end
    btn_go_touch_grass.callback = function()
        swap_mode(k_mode_resting)
        remove_window()
    end
    btn_go_freeze.callback = function()
        swap_mode(k_mode_frozen)
        remove_window()
    end
    btn_reset_dailies.callback = function()
        reset_daily_stats()
    end

    bg.on_click = remove_window
    btn_cancel.callback = remove_window
end

local function show_corrections_screen()
    local grid = secondary_grid
    grid:clear_children()
    grid:set_grid_size(18, 3)
    grid.homogeneous = false

    secondary_window.height = 550
    secondary_window.width = 840

    set_window_title(secondary_window, 'Twitch Marathon Timer - Corrections')

    grid:relayout()
    secondary_window.visible = true
end

local function show_points_log_screen()
    local grid = secondary_grid
    grid:clear_children()
    grid:set_grid_size(12, 6)
    grid.homogeneous = false

    secondary_window.width = 840
    secondary_window.height = 340
    grid:resize(840, 340)

    set_window_title(secondary_window, 'Twitch Marathon Timer - Points Log')

    local lbl
    lbl = widgets.create_label('Time')
    grid:add_child(lbl, 1, 0)
    lbl = widgets.create_label('Reason')
    grid:add_child(lbl, 1, 1)
    lbl = widgets.create_label('User')
    grid:add_child(lbl, 1, 2)
    lbl = widgets.create_label('Points')
    grid:add_child(lbl, 1, 3)
    lbl = widgets.create_label('Added')
    grid:add_child(lbl, 1, 4)
    lbl = widgets.create_label('Grass')
    grid:add_child(lbl, 1, 5)

    local logs = util.retrieve_event_log('points', 10)

    for i = 1, 10 do
        local row = i + 1
        local data = logs[i]
        if data then
            lbl = widgets.create_label(data.time)
            grid:add_child(lbl, row, 0)
            lbl = widgets.create_label(data.message)
            grid:add_child(lbl, row, 1)
            lbl = widgets.create_label(data.user)
            grid:add_child(lbl, row, 2)
            lbl = widgets.create_label(format_float(data.points))
            lbl:set_alignment(enum.ALIGN_CENTER)
            grid:add_child(lbl, row, 3)
            lbl = widgets.create_label(format_time(data.seconds, true))
            grid:add_child(lbl, row, 4)
            lbl = widgets.create_label(data.grass and 'Yes' or 'No')
            lbl:set_alignment(enum.ALIGN_CENTER)
            grid:add_child(lbl, row, 5)
        else
            lbl = widgets.create_label('_')
            grid:add_child(lbl, row, 0)
        end
    end

    grid:relayout()
    secondary_window.visible = true
end

-- Build main window
do
    local grid = main_grid
    grid:clear_children()
    grid:set_grid_size(23, 6)
    grid.homogeneous = true

    set_window_title(main_window, 'Twitch Marathon Timer')

    local row = 0

    local function create_key_value(text, width)
        row = row + 1
        local key = widgets.create_label(text)
        grid:add_child(key, row, 0, 1, 2)
        local value = widgets.create_label('...')
        value.alignment = enum.ALIGN_CENTER
        grid:add_child(value, row, 2, 1, width or 2)
        return value, key
    end

    local function create_stats_value(t1, t2, t3)
        row = row + 1
        if t1 then
            local key = widgets.create_label(t1)
            grid:add_child(key, row, 0, 1, 2)
        end
        local v1 = widgets.create_label(t2 or '...')
        v1.alignment = enum.ALIGN_CENTER
        grid:add_child(v1, row, 2, 1, 2)
        local v2 = widgets.create_label(t3 or '...')
        v2.alignment = enum.ALIGN_CENTER
        grid:add_child(v2, row, 4, 1, 2)
        return v1, v2
    end

    lbl_started_at = create_key_value('Started:', 3)
    lbl_started_at.font = underline_font

    lbl_time_mode_at, lbl_time_mode_at_key = create_key_value('STATE since:', 3)
    lbl_time_in_mode, lbl_time_in_mode_key = create_key_value('Time STATE:', 3)

    row = row + 1

    btn_switch_mode = widgets.create_button('Switch MODE', show_switch_mode_screen)
    grid:add_child(btn_switch_mode, row + 2, 4, 2, 2)

    btn_points_log = widgets.create_button('Points log', show_points_log_screen)
    grid:add_child(btn_points_log, row + 4, 4, 2, 2)

    --btn_corrections = widgets.create_button('Corrections', show_corrections_screen)
    --grid:add_child(btn_corrections, row + 6, 4, 2, 2)

    lbl_time_total = create_key_value('Time total:')
    lbl_time_active = create_key_value('Time active:')
    lbl_time_remaining = create_key_value('Time remaining:')
    lbl_time_resting = create_key_value('Time resting:')
    lbl_time_inactive = create_key_value('Time inactive:')
    lbl_time_buffered = create_key_value('Time buffered:')
    lbl_time_rested = create_key_value('Time rested:')
    lbl_time_frozen = create_key_value('Time frozen:')

    row = row + 1

    local lbl1, lbl2 = create_stats_value(nil, 'Total', 'Today')
    lbl1.font = underline_font
    lbl2.font = underline_font

    lbl_points_total, lbl_points_today = create_stats_value('Points')
    lbl_subs_total, lbl_subs_today = create_stats_value('Subs')
    lbl_bits_total, lbl_bits_today = create_stats_value('Bits')
    lbl_donations_total, lbl_donations_today = create_stats_value('Donations')
    lbl_followers_total, lbl_followers_today = create_stats_value('Followers')
    lbl_redeems_total, lbl_redeems_today = create_stats_value('Channel points')

    -- Not displayed but used for link to obs
    lbl_ratio_points_today = widgets.create_label()

    local function obs_updater(lbl, txt)
        if obs.connected then
            local obs_name = SETTINGS[lbl.settings_name]
            if obs_name then
                obs:SetInputSettings(obs_name, { text = txt or lbl.text })
            end
        end
    end

    local function link_label_to_obs(lbl, settings_name)
        lbl.settings_name = settings_name
        lbl.on_changed = obs_updater
        table.insert(OBS_LABELS, lbl)
    end

    link_label_to_obs(lbl_time_in_mode, 'obs_timer_current_mode')
    link_label_to_obs(lbl_time_total, 'obs_timer_total')
    link_label_to_obs(lbl_time_active, 'obs_timer_active')
    link_label_to_obs(lbl_time_remaining, 'obs_timer_remaining')
    link_label_to_obs(lbl_time_buffered, 'obs_timer_rest_buffer')
    link_label_to_obs(lbl_time_resting, 'obs_timer_rest_active')
    link_label_to_obs(lbl_points_total, 'obs_amount_points_total')
    link_label_to_obs(lbl_points_today, 'obs_amount_points_today')
    link_label_to_obs(lbl_ratio_points_today, 'obs_ratio_points_today')

    row = row + 1

    local btn_reload_stats = widgets.create_button('Reload STATS', function()
        SETTINGS = reload_settings_table()
        STATS = reload_stats_table()
        update_total_daily_stats()
        ratio_bar_widget:redraw()
        rest_bar_widget:redraw()
    end)
    grid:add_child(btn_reload_stats, -1, 0, 1, 2)

    btn_connect_obs = widgets.create_button('OBS')
    if obs.connected then
        btn_connect_obs.bgcolor = color_enabled
    elseif obs.enabled then
        btn_connect_obs.bgcolor = color_connecting
    else
        btn_connect_obs.bgcolor = color_disabled
    end
    grid:add_child(btn_connect_obs, -1, -2)

    btn_connect_twitch = widgets.create_button('Twitch', function(self)
        twitch.enabled = not twitch.enabled
    end)
    local twitch_state = twitch:get_state()
    if twitch_state == twitch.INACTIVE then
        btn_connect_twitch.bgcolor = color_disabled
    elseif twitch_state == twitch.ACTIVE then
        btn_connect_twitch.bgcolor = color_enabled
    else
        btn_connect_twitch.bgcolor = color_connecting
    end
    grid:add_child(btn_connect_twitch, -1, -1)

    local settings_button = widgets.create_button('Settings', function(self)
        show_settings_window()
    end)
    grid:add_child(settings_button, 1, -1, 2, 1)
    grid:relayout()
end

btn_connect_obs.callback = function(self)
    if not SETTINGS.obs_password then
        show_settings_window()
    elseif obs.enabled then
        obs.enabled = false
        self:set_bgcolor(color_disabled)
    else
        obs.password = SETTINGS.obs_password
        obs.enabled = true
        self:set_bgcolor(color_connecting)
    end
end

obs.on_connect_failed = function(self, reason)
    logger(logger.WARNING, 'OBS connection failed: ' .. reason)
    self.connected = false
end
obs.on_connect = function(self)
    logger(logger.INFO, 'OBS connected!')
    btn_connect_obs:set_bgcolor(color_enabled)
    self.connected = true
    for _, lbl in ipairs(OBS_LABELS) do
        lbl:on_changed()
    end
end
obs.on_disconnect = function(self)
    logger(logger.INFO, 'OBS disconnected!')
    btn_connect_obs:set_bgcolor(color_disabled)
    self.connected = false
end

twitch.on_state_changed = function(self, newstate, msg)
    logger(logger.INFO, 'Twitch:', msg)
    if newstate == self.INACTIVE then
        btn_connect_twitch:set_bgcolor(color_disabled)
    elseif newstate == self.ACTIVE then
        btn_connect_twitch:set_bgcolor(color_enabled)
    else
        btn_connect_twitch:set_bgcolor(color_connecting)
    end
end
twitch.on_follow = function(self, channel, user)
    print('FOLLOW', user.name)

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name }
    util.append_event_log('follows', log)

    add_stats({ amount_followers = 1 })
    save_stats()
end
twitch.on_channel_points = function(self, channel, user, points, title, message)
    print('REDEEM', user.name, 'POINTS', points, 'TITLE', title, 'MESSAGE', message)

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name or '(anonymous)', points = points, title = title, message = message }
    util.append_event_log('redeems', log)

    local redeem = {}
    redeem[title] = 1
    add_stats({
        amount_channel_points = points,
        count_channel_points = 1,
        redeems = redeem
    })
    save_stats()
end
twitch.on_bits = function(self, channel, user, bits, bits_type, message)
    print('BITS', user.name, 'TYPE', bits_type, 'AMOUNT', bits, 'MESSAGE', message.text)

    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name, type = bits_type, bits = bits, message = message.text }
    util.append_event_log('bits', log)

    local is_grassy = string.find(message.text, '#') and true or false
    add_bits(ts, user.name, bits, is_grassy)
    save_stats()
end
twitch.on_channel_chat_message = function(self, channel, message_id, user, message, extra)
    local ts = os.time()
    if extra.source and extra.source.name then
        channel = extra.source.name
    else
        channel = nil
    end
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'message', shared_chat = channel, user_name = user.name, message = message.text, message_id = message_id }
    util.append_event_log('chat', log)
end
twitch.on_channel_chat_message_delete = function(self, channel, message_id, user)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), user_name = user.name }
    if message_id then
        log.message_id = message_id
        log.event = 'delete_message'
    else
        log.event = 'clear_user'
    end
    util.append_event_log('chat', log)
end
twitch.on_channel_chat_clear = function(self, channel)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'clear_chat' }
    util.append_event_log('chat', log)
end
twitch.on_channel_chat_notification = function(self, channel, event)
    local ts = os.time()
    local log = { timestamp = ts, time = os.date('%F %T', ts), event = 'notification', notification = event }
    util.append_event_log('chat', log)

    local message = event.message.text
    if not message or message == '' then
        message = event.system_message
    end
    local is_grassy = string.find(message, '#') and true or false
    local log_as_gift = false

    log = {}
    if event.notice_type == 'sub' then
        log.subber = event.chatter_is_anonymous and '(anonymous)' or event.chatter_user_name
        log.tier = subtier(event.sub.sub_tier)
        log.duration = event.sub.duration_months
        log.is_prime = event.sub.is_prime
    elseif event.notice_type == 'resub' then
        log.subber = event.chatter_is_anonymous and '(anonymous)' or event.chatter_user_name
        log.tier = subtier(event.resub.sub_tier)
        log.duration = event.resub.duration_months
        log.cumulative = event.resub.cumulative_months
        log.streak = event.resub.streak_months
        log.is_prime = event.resub.is_prime
        -- Gift resub events happen when the gift was multiple months
        -- We dont log these as gifts because that was last month
        log.is_gift = event.resub.is_gift
        if log.is_gift then
            log.gifter = event.resub.gifter_is_anonymous and '(anonymous)' or event.resub.gifter_user_name
        end
    elseif event.notice_type == 'sub_gift' then
        log.subber = event.sub_gift.recipient_user_name
        log.gifter = event.chatter_is_anonymous and '(anonymous)' or event.chatter_user_name
        log.tier = subtier(event.sub_gift.sub_tier)
        log.duration = event.sub_gift.duration_months
        log.cumulative = event.sub_gift.cumulative_total
        log.community_gift = event.sub_gift.community_gift_id
        log.is_gift = true
        log_as_gift = true
    elseif event.notice_type == 'community_sub_gift' then
        log.gifter = event.chatter_is_anonymous and '(anonymous)' or event.chatter_user_name
        log.tier = subtier(event.community_sub_gift.sub_tier)
        log.amount = event.community_sub_gift.total
        log.cumulative = event.community_sub_gift.cumulative_total
        log.community_gift = event.community_sub_gift.id
        log_as_gift = true
    end

    if log.tier then
        local log_name = log_as_gift and log.gifter or log.subber
        print(string.upper(event.notice_type), log_name, 'TIER', log.tier, 'AMOUNT', log.amount or 1, 'MESSAGE', message)

        log.event = event.notice_type
        log.timestamp = ts
        log.time = os.date('%F %T', ts)
        log.message = message
        util.append_event_log('subs', log)

        -- Giftsubs from a community gift are already counted by the community gift
        if not (log.community_gift and log.is_gift) then
            add_subs(ts, log_name, log.amount or 1, log.tier, log_as_gift, is_grassy)
            save_stats()
        end
    end
end

local function dim_labels_by_mode(mode)
    mode = mode or STATS.mode

    local function when(...)
        local args = { ... }
        for _, arg in ipairs(args) do
            if mode == arg then
                return color_active
            end
        end
        return color_dimmed
    end

    lbl_time_active:set_fgcolor(when(k_mode_active, k_mode_capped, k_mode_finished))
    lbl_time_remaining:set_fgcolor(when(k_mode_unstarted, k_mode_active, k_mode_capped))
    lbl_time_resting:set_fgcolor(when(k_mode_resting))
    lbl_time_inactive:set_fgcolor(when(k_mode_inactive, k_mode_finished))
    lbl_time_rested:set_fgcolor(when(k_mode_resting, k_mode_finished))
    lbl_time_buffered:set_fgcolor(when(k_mode_active, k_mode_inactive))
    lbl_time_frozen:set_fgcolor(when(k_mode_frozen, k_mode_finished))
end

swap_mode = function(new_mode)
    local old_mode = STATS.mode or k_mode_unstarted
    if old_mode == new_mode then
        return
    end

    assert(new_mode ~= k_mode_unstarted, 'INTERNAL ERROR: attempted to switch to UNSTARTED mode')

    local now = os.time()

    if old_mode == k_mode_finished then
        -- Assume we were frozen since the finish time
        assert(STATS.date_finished ~= nil, 'INTERNAL ERROR: mode is FINISHED but date_finished is not set')
        STATS.date_mode_started = STATS.date_finished
        STATS.date_finished = nil
        old_mode = k_mode_frozen
    end

    local elapsed = now - (STATS.date_mode_started or now)

    if old_mode == k_mode_unstarted then
        assert(STATS.date_started == nil, 'INTERNAL ERROR: mode is UNSTARTED but date_started is already set')
        STATS.date_started = now
        lbl_started_at:set_text(format_date(STATS.date_started))
        lbl_time_mode_at:set_text(format_date(STATS.date_started))
    elseif old_mode == k_mode_frozen then
        STATS.time_total_frozen = STATS.time_total_frozen + elapsed
    elseif old_mode == k_mode_inactive then
        STATS.time_total_inactive = STATS.time_total_inactive + elapsed
    elseif old_mode == k_mode_resting then
        local remaining = STATS.time_resting - elapsed
        STATS.time_total_rested = STATS.time_total_rested + elapsed
        if remaining > 0 then
            STATS.time_buffered = STATS.time_buffered + remaining
        end
        STATS.time_resting = nil
    end

    STATS.date_mode_started = now
    STATS.mode = new_mode

    lbl_time_mode_at_key:set_text(new_mode .. ' since')
    lbl_time_mode_at:set_text(format_date(now))
    lbl_time_in_mode_key:set_text('Time ' .. new_mode)

    if new_mode == k_mode_resting then
        local overflow = STATS.time_buffered - SETTINGS.max_rest_timer
        if overflow > 0 then
            STATS.time_rest_overflowed = STATS.time_rest_overflowed + overflow
            STATS.time_buffered = SETTINGS.max_rest_timer
        end
        STATS.time_resting = STATS.time_buffered
        STATS.time_buffered = 0
    elseif new_mode == k_mode_capped then
        STATS.time_rest_overflowed = STATS.time_rest_overflowed + STATS.time_buffered
        STATS.time_buffered = 0
    elseif new_mode == k_mode_finished then
        STATS.date_finished = now
    end

    save_stats()
    dim_labels_by_mode()
end

local function tick(clock)
    local now = os.time()
    local current = STATS.date_mode_started or now
    local elapsed = now - current
    local date_started = STATS.date_started or current

    lbl_time_in_mode:set_text(format_time(elapsed))

    local time_inactive = STATS.time_total_inactive
    local time_buffered = STATS.time_buffered
    local time_rested = STATS.time_total_rested
    local time_frozen = STATS.time_total_frozen
    local time_resting = 0

    local mode = STATS.mode
    if mode == k_mode_inactive then
        time_inactive = time_inactive + elapsed
    elseif mode == k_mode_resting then
        time_resting = STATS.time_resting - elapsed
        time_rested = time_rested + elapsed
    elseif mode == k_mode_frozen then
        time_frozen = time_frozen + elapsed
    end

    local time_total
    if mode == k_mode_finished then
        time_total = STATS.date_finished - date_started - time_frozen
    else
        time_total = now - date_started - time_frozen
    end
    local time_active = time_total - (time_inactive + time_rested)
    local time_remaining = SETTINGS.event_start_offset + STATS.time_added + STATS.time_correction + STATS.time_rest_overflowed - time_active

    if time_buffered > SETTINGS.max_rest_timer then
        time_remaining = time_remaining + (time_buffered - SETTINGS.max_rest_timer)
        time_buffered = SETTINGS.max_rest_timer
    end

    if mode == k_mode_resting then
        rest_bar_widget:set_time(time_resting)
    else
        rest_bar_widget:set_time(time_buffered)
    end

    if mode == k_mode_active and time_remaining <= 0 then
        if time_buffered > 0 then
            swap_mode(k_mode_capped)
            time_remaining = time_remaining + time_buffered
            time_buffered = 0
        else
            swap_mode(k_mode_finished)
        end
    elseif mode == k_mode_capped and time_remaining <= 0 then
        swap_mode(k_mode_finished)
    end

    lbl_time_total:set_text(format_time(time_total))
    lbl_time_active:set_text(format_time(time_active))
    lbl_time_remaining:set_text(format_time(time_remaining))
    lbl_time_resting:set_text(format_time(time_resting))
    lbl_time_inactive:set_text(format_time(time_inactive))
    lbl_time_buffered:set_text(format_time(time_buffered))
    lbl_time_rested:set_text(format_time(time_rested))
    lbl_time_frozen:set_text(format_time(time_frozen))
end

-- Initial setup
do
    if STATS.date_started then
        STATS.mode = STATS.mode or k_mode_frozen
        lbl_started_at:set_text(format_date(STATS.date_started))
        lbl_time_mode_at:set_text(format_date(STATS.date_mode_started))
    else
        STATS.mode = k_mode_unstarted
        lbl_started_at:set_text('Not Started')
    end

    local mode = STATS.mode
    lbl_time_mode_at_key:set_text(mode .. ' since')
    lbl_time_in_mode_key:set_text('Time ' .. mode)

    update_total_daily_stats()
    dim_labels_by_mode()
    tick(-1)
end

return {
    connectors = {
        obs,
        twitch,
        main_window,
        secondary_window,
        ratio_bar,
        rest_bar,
    },
    tick = tick
}
