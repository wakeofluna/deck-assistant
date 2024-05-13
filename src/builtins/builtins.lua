local deck = require('deck')
local logger = require('deck.logger')
--local util = require('deck.util')

local default_font = deck:Font { 24, deck:Colour 'White' }
--local title_font = default_font
--local dir_font = default_font
--local log_font = default_font


local function default_container()
    local self = {}

    self.background = deck:Colour '#00000000'
    self.children = deck:RectangleList()

    self._find_child = function(self, widget)
        local find_callback = function(rect, widget)
            return rect.widget == widget
        end
        return self.children:foreach(find_callback, widget)
    end

    self._assign_child_rect = function(self, child)
        -- Optionally overriden by descendent class
        -- Function called to assign child space after the container resized
        child.x = 0
        child.y = 0
        child.width = self.card.width
        child.height = self.card.height
    end

    self._resize_child = function(self, child)
        -- Optionally overriden by descendent class
        -- Function called to forward resize events to the child widget
        local widget = child.widget
        if widget.resize then
            widget:resize(child.width, child.height)
        end
    end

    self._update_child_rect = function(self, child)
        -- Optionally overriden by descendent class
        -- Function called to react to size of widget after the widget resized
        local card = child.widget.card
        if card then
            if card.width < child.width then
                child.x = child.x + math.floor((child.width - card.width) / 2)
                child.width = card.width
            end
            if card.height < child.height then
                child.y = child.y + math.floor((child.height - card.height) / 2)
                child.height = card.height
            end
        end
    end

    local child_update_cb = function(widget, rect)
        if not self.card then
            return
        end

        local child = self:_find_child(widget)
        if child and child.widget.card then
            local target_rect = child.dup
            if rect then
                target_rect.x = target_rect.x + rect.x
                target_rect.y = target_rect.y + rect.y
                target_rect.width = rect.width
                target_rect.height = rect.height
                target_rect:clip(child)
            end
            self:redraw(target_rect)
        end
    end

    self._add_child = function(self, widget, properties)
        assert(widget)

        local child = deck:Rectangle()
        child.widget = widget

        if properties then
            for k, v in pairs(properties) do
                child[k] = v
            end
        end

        self:remove_child(widget)
        self.children:add(child)

        widget.on_update = child_update_cb

        if self.card then
            self:_assign_child_rect(child)
            widget:resize(child.width, child.height)
            self:_update_child_rect(child)
            widget:on_update()
        end
    end

    -- Optionally overriden by descendent class
    self.add_child = self._add_child

    self.remove_child = function(self, widget)
        local child = self:_find_child(widget)
        if child then
            widget.on_update = nil
            self.children:remove(child)
            self:redraw()
        end
    end

    self.resize = function(self, width, height)
        local resize_callback = function(child)
            self:_assign_child_rect(child)
            if child.widget.resize then
                child.widget:resize(child.width, child.height)
            end
            self:_update_child_rect(child)
        end

        if not self.card or self.card.width ~= width or self.card.height ~= height then
            self.card = deck:Card(width, height)
            self.children:foreach(resize_callback)
            self:redraw()
        end
    end

    self.redraw = function(self, rect)
        if self.card then
            self.card:clear(self.background)
            self.children:foreach(function(child)
                if child.widget.card then
                    self.card:sub_card(child):blit(child.widget.card)
                end
            end)
            if self.on_update then
                self:on_update(rect)
            end
        end
    end

    self.mouse_motion = function(self, x, y)
        local hotspot = self.children:reverse_any(x, y)

        if self._hotspot and self._hotspot ~= hotspot then
            if self._hotspot.widget.mouse_left then
                self._hotspot.widget:mouse_left()
            end
            self._hotspot = nil
        end

        if self._mousegrab and hotspot ~= self._mousegrab then
            hotspot = nil
        end

        if hotspot and hotspot ~= self._hotspot then
            if hotspot.widget.mouse_entered then
                hotspot.widget:mouse_entered()
            end
            self._hotspot = hotspot
        end

        if self._mousegrab then
            hotspot = self._mousegrab
        end

        if hotspot and hotspot.widget.mouse_motion then
            x = x - hotspot.x
            y = y - hotspot.y
            hotspot.widget:mouse_motion(x, y)
        end
    end

    self.mouse_left = function(self)
        if self._hotspot then
            if self._hotspot.widget.mouse_left then
                self._hotspot.widget:mouse_left()
            end
            self._hotspot = nil
        end
    end

    self.mouse_button = function(self, x, y, button, pressed)
        local hotspot

        if self._mousegrab then
            hotspot = self._mousegrab
        else
            hotspot = self.children:reverse_any(x, y)
        end

        local dograb = false

        if hotspot and hotspot.widget.mouse_button then
            x = x - hotspot.x
            y = y - hotspot.y
            dograb = hotspot.widget:mouse_button(x, y, button, pressed)
        end

        if not pressed or not dograb then
            self._mousegrab = nil
        else
            self._mousegrab = hotspot
        end

        return dograb
    end

    self.mouse_scroll = function(self, x, y, scroll_x, scroll_y)
        local hotspot = self.children:reverse_any(x, y)
        if hotspot and hotspot.widget.mouse_scroll then
            x = x - hotspot.x
            y = y - hotspot.y
            hotspot.widget:mouse_scroll(x, y, scroll_x, scroll_y)
        end
    end

    return self
end

local function create_window_manager()
    local self = default_container()

    self.background = deck:Colour 'Black'

    self._update_child_rect = function(self, child)
        if child.widget.card then
            child.width = child.widget.card.width
            child.height = child.widget.card.height
            if child.pos_x > 1 then
                child.x = child.pos_x
            else
                child.x = math.floor((self.card.width - child.width) * child.pos_x)
            end
            if child.pos_y > 1 then
                child.y = child.pos_y
            else
                child.y = math.floor((self.card.height - child.height) * child.pos_y)
            end
        end
    end

    self.add_child = function(self, widget, pos_x, pos_y)
        assert(widget)
        pos_x = pos_x ~= nil and pos_x or 0.5
        pos_y = pos_y ~= nil and pos_y or 0.5
        self:_add_child(widget, { pos_x = pos_x, pos_y = pos_y })
    end

    return self
end

local function create_widget_grid(count_x, count_y, padding, margin)
    local self = default_container()

    assert(count_x > 0)
    assert(count_y > 0)
    self.count_x = count_x
    self.count_y = count_y
    self.padding = padding or 5
    self.margin = margin or self.padding

    self._assign_child_rect = function(self, child)
        local total_padding_x = (self.count_x - 1) * self.padding + 2 * self.margin
        local total_padding_y = (self.count_y - 1) * self.padding + 2 * self.margin
        local cell_x = math.floor((self.card.width - total_padding_x) / self.count_x)
        local cell_y = math.floor((self.card.height - total_padding_y) / self.count_y)

        child.left = (self.padding + cell_x) * child.pos_x + self.margin
        child.top = (self.padding + cell_y) * child.pos_y + self.margin
        child.right = (self.padding + cell_x) * (child.pos_x + child.span_x) + self.margin - self.padding
        child.bottom = (self.padding + cell_y) * (child.pos_y + child.span_y) + self.margin - self.padding
    end

    self.add_child = function(self, widget, pos_x, pos_y, span_x, span_y)
        assert(pos_x >= 0 and pos_x < self.count_x)
        assert(pos_y >= 0 and pos_y < self.count_y)
        span_x = (span_x ~= nil and span_x > 1) and span_x or 1
        span_y = (span_y ~= nil and span_y > 1) and span_y or 1
        self:_add_child(widget, { pos_x = pos_x, pos_y = pos_y, span_x = span_x, span_y = span_y })
    end

    return self
end

local function connect(widget, ...)
    local targets = { ... }
    local count = #targets

    assert(count > 0)

    if widget.mouse_motion then
        local f = function(connector, x, y)
            widget:mouse_motion(x, y)
        end
        for _, connector in ipairs(targets) do
            connector.on_mouse_motion = f
        end
    end

    if widget.mouse_button then
        local f = function(connector, x, y, button, pressed)
            widget:mouse_button(x, y, button, pressed)
        end
        for _, connector in ipairs(targets) do
            connector.on_mouse_button = f
        end
    end

    if widget.mouse_scroll then
        local f = function(connector, x, y, scroll_x, scroll_y)
            widget:mouse_scroll(x, y, scroll_x, scroll_y)
        end
        for _, connector in ipairs(targets) do
            connector.on_mouse_scroll = f
        end
    end

    if widget.resize then
        local f = function(connector)
            if connector.pixel_width then
                widget:resize(connector.pixel_width, connector.pixel_height)
            else
                widget:resize(connector.width, connector.height)
            end
            if widget.redraw then
                widget:redraw()
            end
            connector.card = widget.card
        end
        for _, connector in ipairs(targets) do
            connector.on_resize = f
        end
    end

    if count > 1 then
        widget.on_update = function(widget, rect)
            for _, connector in ipairs(targets) do
                connector:redraw(rect)
            end
        end
    else
        local connector = targets[1]
        widget.on_update = function(widget, rect)
            connector:redraw(rect)
        end
    end
end

-- Banged my keyboard for some unique integers
local BUTTON_NORMAL = 68134
local BUTTON_PRESSED = 912087
local BUTTON_HOVERED = 10955

local function create_button(label, callback)
    local btn = {}

    if not callback then
        logger(logger.WARNING, 'Button \'', label, '\' does not have a callback function')
    end

    btn.background = deck:Colour 'Blue'
    btn.label = label
    btn.font = default_font
    btn.callback = callback
    btn.dirty = true
    btn.width = 400
    btn.height = 200

    -- Internal states
    btn._internal_state = BUTTON_NORMAL

    btn.update_state = function(self, newstate)
        assert(newstate == BUTTON_NORMAL or newstate == BUTTON_PRESSED or newstate == BUTTON_HOVERED)
        if self._internal_state ~= newstate then
            self._internal_state = newstate
            self:redraw()
        end
    end

    btn.mouse_button = function(self, x, y, button, pressed)
        if button == 1 then
            if pressed then
                self.is_pressed = true
                self:update_state(BUTTON_PRESSED)
                return true
            end

            if self._internal_state == BUTTON_PRESSED then
                self:update_state(BUTTON_HOVERED)

                local ok, msg = pcall(self.callback, self)
                if not ok then
                    logger(logger.ERROR, 'Button callback:', msg)
                end
            else
                self:update_state(BUTTON_NORMAL)
            end

            self.is_pressed = false
        end
        return false
    end

    btn.mouse_entered = function(self)
        if self.is_pressed then
            self:update_state(BUTTON_PRESSED)
        else
            self:update_state(BUTTON_HOVERED)
        end
    end

    btn.mouse_left = function(self)
        self:update_state(BUTTON_NORMAL)
    end

    btn.resize = function(self, width, height)
        if not self.card or self.card.width ~= width or self.card.height ~= height then
            self.width = width
            self.height = height
            self:redraw(true)
        end
    end

    btn.redraw = function(self, force)
        if not self.normal or force then
            local txt = self.font:render(self.label)
            self.normal = deck:Card(self.width, self.height)
            self.normal:clear(self.background)
            self.normal:blit(txt, txt:centered(self.normal))
            self.hover = self.normal.dup:lighten(40)
            self.pressed = self.normal.dup:darken(40)
        end

        if self._internal_state == BUTTON_PRESSED then
            self.card = self.pressed
        elseif self._internal_state == BUTTON_HOVERED then
            self.card = self.hover
        else
            self.card = self.normal
        end

        if self.on_update then
            self:on_update()
        end
    end

    return btn
end

local exports = {}
exports.default_container = default_container
exports.default_font = default_font
exports.create_window_manager = create_window_manager
exports.create_widget_grid = create_widget_grid
exports.create_button = create_button
exports.connect = connect
return exports

--[[

window = deck:Connector('Window', 'DeckMainWindow', { width = 1000, height = 600 })
--window = deck:Connector('Vnc', 'DeckMainWindow', { width = 1000, height = 600 })
window.title = 'Deck Assistant'

local toplevel = create_window_manager()

connect(toplevel, window)
local grid = create_widget_grid(3, 2, 15, 10)
toplevel:add_child(grid)

local btn
btn = create_button('1', function(self) print('HELLO 1') end)
grid:add_child(btn, 0, 0)

btn = create_button('2')
grid:add_child(btn, 2, 0)

btn = create_button('3')
grid:add_child(btn, 1, 1, 2, 1)

local grid2 = create_widget_grid(2, 2, nil, 0)
grid:add_child(grid2, 1, 0)

btn = create_button('4')
grid2:add_child(btn, 0, 0)
btn = create_button('5')
grid2:add_child(btn, 1, 1)


--[[
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
