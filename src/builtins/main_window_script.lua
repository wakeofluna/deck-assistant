local deck = require('deck')
local logger = require('deck.logger')
local builtins = require('builtins')

window = deck:Connector('Window', 'DeckMainWindow', { width = 1000, height = 600 })
window.title = 'Deck Assistant'

local toplevel = builtins.create_window_manager()
builtins.connect(toplevel, window)

--[[
local grid = builtins.create_widget_grid(3, 2, 15, 10)
toplevel:add_child(grid)

local main_area = nil
local log_area = nil
local log_messages = {}
local files_card = nil
local files_cur_dir = '.'
local files_entries = {}
local files_max_size = 0
local files_max_date = 0
local files_scroll = 0
local files_hotspots = deck:RectangleList()

local function list_directory(relative)
    local new_dir = files_cur_dir .. '/' .. relative
    local listing = util.ls(new_dir)
    files_cur_dir = listing.absolute
    files_entries = {}

    local tail = string.sub(files_cur_dir, -2)
    if tail == '/.' or tail == '\\.' then
        files_cur_dir = string.sub(files_cur_dir, 0, -3)
    end

    local dir_card = dir_font:render('[DIR]')

    for _, d in ipairs(listing.subdirs) do
        local name = dir_font:render(d)
        table.insert(files_entries, { false, name, dir_card })
    end

    for _, f in ipairs(listing.files) do
        local name = dir_font:render(f[1])
        local size = dir_font:render(tostring(f[2]))
        local date = dir_font:render(os.date('%Y/%m/%d %H:%M:%S'), f[3])
        table.insert(files_entries, { true, name, size, date })
    end
end

list_directory('.')

local function render_directory_area()
    if not main_area then
        return
    end

    local h_spacing = 20
    local v_spacing = 5

    local total_height = 5
    for _, v in ipairs(files_entries) do
        total_height = total_height + v[2].height + v_spacing
        if v[3] and v[3].width > files_max_size then
            files_max_size = v[3].width
        end
        if v[4] and v[4].width > files_max_date then
            files_max_date = v[4].width
        end
    end

    local date_field_offset = main_area.width - files_max_date - 10
    local size_field_offset = date_field_offset - h_spacing
    local name_field_offset = 10

    files_card = deck:Card(main_area.width, total_height)
    files_hotspots:clear()

    local top = 5
    for _, v in ipairs(files_entries) do
        files_card:blit(v[2], name_field_offset, top)
        files_card:blit(v[3], size_field_offset - v[3].width, top)
        if v[4] then
            files_card:blit(v[4], date_field_offset, top)
        end
        files_hotspots:add(deck:Rectangle(0, top, files_card.width, v[2].height))
        top = top + v[2].height + v_spacing
    end
end

local function render_main_area()
    if not main_area then
        return
    end

    main_area:clear(deck:Colour '#000000')

    local txt = dir_font:render('UP')
    local rect = deck:Rectangle(5, 5, 50, 40)
    local card = main_area:sub_card(rect)
    card:clear(deck:Colour 'darkred')
    card:blit(txt, txt:centered(card))

    rect.left = rect.right
    rect.right = main_area.width
    card = main_area:sub_card(rect)
    txt = dir_font:render(files_cur_dir)
    card:blit(txt, 10, (card.h - txt.h) / 2)

    rect = deck:Rectangle(0, rect.bottom, main_area.width, main_area.height)
    card = main_area:sub_card(rect)

    if files_card then
        if files_card.height <= card.height then
            card:blit(files_card)
        else
            local srcrect = deck:Rectangle(0, files_scroll, files_card.width, card.height)
            card:blit(files_card:sub_card(srcrect))
        end
    end
end

local function render_logmessages()
    if not log_area then
        return
    end

    log_area:clear(deck:Colour '#222222')

    local top = 5

    for _, v in ipairs(log_messages) do
        if top > log_area.height then
            break
        end

        local txt = log_font:render(v, log_area.width)
        log_area:blit(txt, 5, top)
        top = top + txt.height + 5
    end

    window:redraw()
end

logger.on_message = function(level, msg)
    local dt = os.date('[%H:%M:%S] ')

    local prefix
    if level == logger.DEBUG then
        prefix = '[DEBUG] '
    elseif level == logger.WARNING then
        prefix = '[WARNING] '
    elseif level == logger.WARNING then
        prefix = '[ERROR] '
    else
        prefix = ''
    end

    table.insert(log_messages, 1, dt .. prefix .. msg)
    render_logmessages()
end

--logger.min_level = logger.DEBUG

window.on_resize = function(win)
    win.card = deck:Card(win.pixel_width, win.pixel_height)
    win.card:clear(deck:Colour 'Black')
    win.hotspots:clear()

    local txt = title_font:render('Deck Assistant')
    local rect = deck:Rectangle(0, 0, win.card.width, 50)
    local card = win.card:sub_card(rect)
    card:clear(deck:Colour 'Blue')
    card:blit(txt, txt:centered(card))

    rect.y = rect.bottom
    rect.bottom = win.card.height - 250

    main_area = win.card:sub_card(rect)
    render_directory_area()
    render_main_area()

    -- todo

    rect.y = rect.bottom
    rect.height = 50

    local txt = title_font:render('Logger messages')
    local card = win.card:sub_card(rect)
    card:clear(deck:Colour 'Blue')
    card:blit(txt, txt:centered(card))

    rect.y = rect.bottom
    rect.bottom = win.card.height

    log_area = win.card:sub_card(rect)
    render_logmessages()
end

--]]
