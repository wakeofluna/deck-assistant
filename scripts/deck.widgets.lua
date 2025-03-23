local deck = require('deck')
local logger = require('deck.logger')
--local util = require('deck.util')

local default_font = deck:Font { 24, deck:Colour 'White' }
--local title_font = default_font
--local dir_font = default_font
--local log_font = default_font


local function font_preferred_size(font, text, margin)
    assert(font, 'no font provided for font_preferred_size')

    if margin == nil then
        margin = 5
    end

    local txt
    if text and text ~= '' then
        txt = font:render(text, ALIGN_LEFT)
        return txt.width + margin * 2, txt.height + margin * 2
    else
        txt = font:render('F', ALIGN_LEFT)
        return margin * 2, txt.height + margin * 2
    end
end


local function default_container()
    local self = {}

    self.bgcolor = deck:Colour 'Transparent'
    self.children = deck:RectangleList()
    self.visible = true

    self._find_child = function(self, widget)
        local find_callback = function(rect, widget)
            return rect.widget == widget
        end
        return self.children:foreach(find_callback, widget)
    end

    self._find_hotspot = function(self, x, y)
        local options = self.children:all(x, y)
        local hotspot = nil

        for _, option in ipairs(options) do
            if option.widget.visible == nil or option.widget.visible then
                hotspot = option
            end
        end

        return hotspot
    end

    self._assign_child_rect = function(self, child)
        -- Optionally overriden by descendent class
        -- Function called to assign child space after the container resized
        local child_w, child_h
        if not child.widget.expand and child.widget.get_preferred_size then
            child_w, child_h = child.widget:get_preferred_size()
            child_w = math.min(self.card.width, child_w)
            child_h = math.min(self.card.height, child_h)
        else
            child_w, child_h = self.card.width, self.card.height
        end
        child.x = (self.card.width - child_w) / 2
        child.y = (self.card.height - child_h) / 2
        child.width = child_w
        child.height = child_h
    end

    self._relayout = function(self)
        if self.card then
            local resize_callback = function(child)
                self:_assign_child_rect(child)
                if child.width > 0 and child.height > 0 then
                    child.widget:resize(child.width, child.height)
                end
            end
            self.children:foreach(resize_callback)
            self:redraw()
        end
    end

    -- Optionally overriden by descendent class
    self.relayout = self._relayout

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
        assert(widget, "invalid widget in add_child")

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
            child.widget:resize(child.width, child.height)
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
            self:redraw(child)
        end
    end

    self.clear_children = function(self)
        local clear_callback = function(rect)
            rect.widget.on_update = nil
        end
        self.children:foreach(clear_callback)
        self.children:clear()
    end

    self._resize = function(self, width, height)
        if not self.card or self.card.width ~= width or self.card.height ~= height then
            self.card = deck:Card(width, height)
            self:relayout()
        end
    end

    -- Optionally overriden by descendent class
    self.resize = self._resize

    self.redraw = function(self, rect)
        if self.card then
            self.card:clear(self.bgcolor)
            if self.redraw_self then
                self:redraw_self()
            end
            self.children:foreach(function(child)
                if child.widget.card and child.widget.visible then
                    self.card:sub_card(child):blit(child.widget.card)
                end
            end)
            if self.on_update then
                self:on_update(rect)
            end
        end
    end

    self.mouse_motion = function(self, x, y)
        local hotspot = self:_find_hotspot(x, y)

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
            local hotspot = self._hotspot
            self._hotspot = nil
            if hotspot.widget.mouse_left then
                hotspot.widget:mouse_left()
            end
        end
    end

    self.mouse_button = function(self, x, y, button, pressed)
        local hotspot

        if self._mousegrab then
            hotspot = self._mousegrab
        else
            hotspot = self:_find_hotspot(x, y)
        end

        local widget = hotspot and hotspot.widget

        if pressed then
            if self._focus and self._focus.blur and self._focus ~= widget then
                self._focus:blur()
            end
            self._mousegrab = hotspot
            self._focus = widget
        else
            self._mousegrab = nil
        end

        if widget and widget.mouse_button then
            x = x - hotspot.x
            y = y - hotspot.y
            widget:mouse_button(x, y, button, pressed)
        end
    end

    self.mouse_scroll = function(self, x, y, scroll_x, scroll_y)
        local hotspot = self:_find_hotspot(x, y)
        if hotspot and hotspot.widget.mouse_scroll then
            x = x - hotspot.x
            y = y - hotspot.y
            hotspot.widget:mouse_scroll(x, y, scroll_x, scroll_y)
        end
    end

    self.blur = function(self)
        if self._focus and self._focus.blur then
            self._focus:blur()
        end
        self._focus = nil
    end

    self.key_down = function(self, mods, keysym, scancode)
        if self._focus and self._focus.key_down then
            self._focus:key_down(mods, keysym, scancode)
        end
    end

    self.key_press = function(self, mods, keysym, scancode)
        if self._focus and self._focus.key_press then
            self._focus:key_press(mods, keysym, scancode)
        end
    end

    self.key_up = function(self, mods, keysym, scancode)
        if self._focus and self._focus.key_up then
            self._focus:key_up(mods, keysym, scancode)
        end
    end

    self.text_input = function(self, text)
        if self._focus and self._focus.text_input then
            self._focus:text_input(text)
        end
    end

    return self
end

local function create_window_manager()
    local self = default_container()

    self.bgcolor = deck:Colour 'Black'

    self._assign_child_rect = function(self, child)
        if not child.widget.expand and child.widget.get_preferred_size then
            local child_w, child_h = child.widget:get_preferred_size()
            child.width = math.min(self.card.width, child_w)
            child.height = math.min(self.card.height, child_h)
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
        else
            child.x = 0
            child.y = 0
            child.width = self.card.width
            child.height = self.card.height
        end
    end

    self.add_child = function(self, widget, pos_x, pos_y)
        pos_x = pos_x ~= nil and pos_x or 0.5
        pos_y = pos_y ~= nil and pos_y or 0.5
        self:_add_child(widget, { pos_x = pos_x, pos_y = pos_y })
    end

    return self
end

local function create_grid(rows, cols, padding, margin)
    local wgr = default_container()

    assert(rows > 0, 'invalid nr of rows in create_grid')
    assert(cols > 0, 'invalid nr of columns in create_grid')
    wgr.rows = rows
    wgr.cols = cols
    wgr.padding = padding or 5
    wgr.margin = margin or wgr.padding
    wgr.homogeneous = true

    wgr._row_sizes = {}
    wgr._col_sizes = {}
    wgr._total_row_size = 0
    wgr._total_col_size = 0
    wgr._max_row_size = 0
    wgr._max_col_size = 0

    wgr._assign_child_rect = function(self, child)
        local total_padding_x = (self.cols - 1) * self.padding + 2 * self.margin
        local total_padding_y = (self.rows - 1) * self.padding + 2 * self.margin
        if self.homogeneous then
            local cell_w = math.floor((self.card.width - total_padding_x) / self.cols)
            local cell_h = math.floor((self.card.height - total_padding_y) / self.rows)

            child.left = (self.padding + cell_w) * child.col + self.margin
            child.top = (self.padding + cell_h) * child.row + self.margin
            child.right = (self.padding + cell_w) * (child.col + child.col_span) + self.margin - self.padding
            child.bottom = (self.padding + cell_h) * (child.row + child.row_span) + self.margin - self.padding
        else
            local scale_x = (self.card.width - total_padding_x) / self._total_col_size
            local scale_y = (self.card.height - total_padding_y) / self._total_row_size

            local start_x = 0
            local start_y = 0
            local size_x = 0
            local size_y = 0
            for i, x in pairs(self._col_sizes) do
                if i < child.col then
                    start_x = start_x + x
                elseif i < child.col + child.col_span then
                    size_x = size_x + x
                end
            end
            for i, y in pairs(self._row_sizes) do
                if i < child.row then
                    start_y = start_y + y
                elseif i < child.row + child.row_span then
                    size_y = size_y + y
                end
            end
            child.x = self.margin + (self.padding * child.col) + math.floor(start_x * scale_x)
            child.y = self.margin + (self.padding * child.row) + math.floor(start_y * scale_y)
            child.width = (child.col_span - 1) * self.padding + math.floor(size_x * scale_x)
            child.height = (child.row_span - 1) * self.padding + math.floor(size_y * scale_y)
        end
    end

    wgr.add_child = function(self, widget, row, col, row_span, col_span)
        if row < 0 then
            row = self.rows + row
        end
        if col < 0 then
            col = self.cols + col
        end
        assert(col >= 0 and col < self.cols, 'invalid column in widget_grid add_child')
        assert(row >= 0 and row < self.rows, 'invalid row in widget_grid add_child')
        col_span = (col_span ~= nil and col_span > 1) and col_span or 1
        row_span = (row_span ~= nil and row_span > 1) and row_span or 1
        self:_add_child(widget, { row = row, col = col, row_span = row_span, col_span = col_span })
    end

    wgr.set_grid_size = function(self, rows, cols)
        assert(rows > 0, 'invalid nr of rows in widget_grid set_grid_size')
        assert(cols > 0, 'invalid nr of columns in widget_grid set_grid_size')
        if self.rows ~= rows or self.cols ~= cols then
            self.rows = rows
            self.cols = cols
            self:relayout()
        end
    end

    wgr.relayout = function(self)
        self:_calc_row_col_sizes()
        self:_relayout()
    end

    wgr._calc_row_col_sizes = function(self)
        local child_data = {}
        local query_callback = function(child)
            if child.widget.get_preferred_size then
                local child_w, child_h = child.widget:get_preferred_size()
                table.insert(child_data, { col = child.col, row = child.row, col_span = child.col_span, row_span = child.row_span, width = child_w, height = child_h })
            end
        end
        self.children:foreach(query_callback)

        self._row_sizes = {}
        self._col_sizes = {}

        local done = false
        local span_count = 0
        while not done do
            done = true
            span_count = span_count + 1
            for _, data in ipairs(child_data) do
                if data.col_span == span_count then
                    local given = 0
                    for x = data.col, (data.col + data.col_span - 1) do
                        given = given + (self._col_sizes[x] or 0)
                    end
                    local needed = data.width - (data.col_span - 1) * self.padding
                    if needed > given then
                        if given == 0 then
                            local value = math.ceil(needed / data.col_span)
                            for x = data.col, (data.col + data.col_span - 1) do
                                self._col_sizes[x] = value
                            end
                        else
                            local stretch = needed / given
                            for x = data.col, (data.col + data.col_span - 1) do
                                self._col_sizes[x] = math.ceil((self._col_sizes[x] or 0) * stretch)
                            end
                        end
                    end
                end
                if data.row_span == span_count then
                    local given = 0
                    for y = data.row, (data.row + data.row_span - 1) do
                        given = given + (self._row_sizes[y] or 0)
                    end
                    local needed = data.height - (data.row_span - 1) * self.padding
                    if needed > given then
                        if given == 0 then
                            local value = math.ceil(needed / data.row_span)
                            for y = data.row, (data.row + data.row_span - 1) do
                                self._row_sizes[y] = value
                            end
                        else
                            local stretch = needed / given
                            for y = data.row, (data.row + data.row_span - 1) do
                                self._row_sizes[y] = math.ceil((self._row_sizes[y] or 0) * stretch)
                            end
                        end
                    end
                end
                if data.col_span > span_count then
                    done = false
                end
                if data.row_span > span_count then
                    done = false
                end
            end
        end

        self._max_row_size = 0
        self._max_col_size = 0
        self._total_row_size = 0
        self._total_col_size = 0
        for _, x in pairs(self._col_sizes) do
            self._max_col_size = math.max(self._max_col_size, x)
            self._total_col_size = self._total_col_size + x
        end
        for _, y in pairs(self._row_sizes) do
            self._max_row_size = math.max(self._max_row_size, y)
            self._total_row_size = self._total_row_size + y
        end
    end

    wgr.get_preferred_size = function(self)
        self:_calc_row_col_sizes()
        local total_w = self.margin * 2 + self.padding * (self.cols - 1)
        local total_h = self.margin * 2 + self.padding * (self.rows - 1)
        if self.homogeneous then
            total_w = total_w + self._max_col_size * self.cols
            total_h = total_h + self._max_row_size * self.rows
        else
            total_w = total_w + self._total_col_size
            total_h = total_h + self._total_row_size
        end
        return total_w, total_h
    end

    return wgr
end

local function create_border(size, color, initial_widget)
    local self = default_container()

    self.size = size or 2
    self.margin = 3
    self.color = color or deck:Colour 'Red'

    self._assign_child_rect = function(self, child)
        local total_margin = self.size + self.margin
        child.top = total_margin
        child.left = total_margin
        child.right = self.card.width - total_margin
        child.bottom = self.card.height - total_margin
    end

    self.add_child = function(self, widget)
        self:clear_children()
        if widget then
            self:_add_child(widget)
        end
    end

    self.get_preferred_size = function(self)
        local w, h
        local child = self.children.first
        if child and child.widget.get_preferred_size then
            w, h = child.widget:get_preferred_size()
        else
            w, h = 1, 1
        end
        local total_margin = (self.size + self.margin) * 2
        return w + total_margin, h + total_margin
    end

    self.redraw_self = function(self, force)
        local w, h, sz = self.card.width, self.card.height, self.size
        -- top
        self.card:sub_card(0, 0, w - sz, sz):clear(self.color)
        -- right
        self.card:sub_card(w - sz, 0, sz, h - sz):clear(self.color)
        -- bottom
        self.card:sub_card(sz, h - sz, w - sz, sz):clear(self.color)
        -- left
        self.card:sub_card(0, sz, sz, h - sz):clear(self.color)
    end

    if initial_widget then
        self:_add_child(initial_widget)
    end

    return self
end

local function create_vbox(padding, margin)
    local vbx = default_container()

    vbx.padding = padding or 5
    vbx.margin = margin or vbx.padding

    return vbx
end

local function create_hbox(padding, margin)
    local hbx = default_container()

    hbx.padding = padding or 5
    hbx.margin = margin or hbx.padding
    hbx._expanding_size = 0
    hbx._child_width = 0
    hbx._child_height = 0

    hbx._probe_widget_properties = function(self, widget)
        local min_width, min_height, expand
        if widget.get_preferred_size then
            min_width, min_height = widget:get_preferred_size()
            expand = widget.expand
        else
            min_width = 0
            min_height = 0
            expand = true
        end
        return { min_width = min_width, min_height = min_height, expand = expand }
    end

    hbx._update_child_sizes = function(self)
        self._expanding_size = 0
        self._child_width = 0
        self._child_height = 0
        local query_callback = function(child)
            local props = self:_probe_widget_properties(child.widget)
            for k, v in pairs(props) do
                child[k] = v
            end
            self._expanding_size = self._expanding_size + (props.expand and props.min_width or 0)
            self._child_width = self._child_width + props.min_width
            self._child_height = math.max(self._child_height, props.min_height)
        end
        self.children:foreach(query_callback)
    end

    hbx._assign_child_rect = function(self, child)
        local total_padding = self.margin * 2 + (self.children.count - 1) * self.padding
        local available = self.card.width - total_padding
        local remaining = available - self._child_width

        local offset = self.margin
        local query_callback = function(rect)
            if rect == child then
                return true
            end
            if rect.expand and remaining > 0 then
                offset = offset + rect.min_width + math.floor((rect.min_width / self._expanding_size) * remaining)
            elseif remaining < 0 then
                offset = offset + rect.min_width + (remaining / self._child_width)
            else
                offset = offset + rect.min_width
            end
            offset = offset + self.padding
        end
        self.children:foreach(query_callback)

        child.left = offset

        if child.expand and remaining > 0 then
            child.width = child.min_width + math.floor((child.min_width / self._expanding_size) * remaining)
        elseif remaining < 0 then
            child.width = child.min_width + (remaining / self._child_width)
        else
            child.width = child.min_width
        end

        child.top = self.margin
        child.bottom = self.card.height - self.margin
    end

    hbx.add_child = function(self, widget)
        local props = self:_probe_widget_properties(widget)
        self._expanding_size = self._expanding_size + (props.expand and props.min_width or 0)
        self._child_width = self._child_width + props.min_width
        self._child_height = math.max(self._child_height, props.min_height)
        self:_add_child(widget, props)
    end

    hbx.relayout = function(self)
        self:_update_child_sizes()
        self:_relayout()
    end

    hbx.get_preferred_size = function(self)
        local size_x = self._child_width + self.margin * 2 + (self.children.count - 1) * self.padding
        local size_y = self._child_height + self.margin * 2
        return size_x, size_y
    end

    return hbx
end

local function disconnect(connector)
    if connector.main_widget then
        connector.main_widget.on_update = nil
        connector.main_widget = nil
    end
    connector.on_mouse_motion = nil
    connector.on_mouse_button = nil
    connector.on_mouse_scroll = nil
    connector.on_key_down = nil
    connector.on_key_press = nil
    connector.on_key_up = nil
    connector.on_text_input = nil
    connector.on_resize = nil
end

local function connect(widget, ...)
    local targets = { ... }
    local count = #targets

    assert(count > 0, 'no targets provided for connect()')

    for _, connector in ipairs(targets) do
        disconnect(connector)
        connector.main_widget = widget
    end

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

    if widget.key_down then
        local f = function(connector, mods, keysym, scancode)
            widget:key_down(mods, keysym, scancode)
        end
        for _, connector in ipairs(targets) do
            connector.on_key_down = f
        end
    end

    if widget.key_press then
        local f = function(connector, mods, keysym, scancode)
            widget:key_press(mods, keysym, scancode)
        end
        for _, connector in ipairs(targets) do
            connector.on_key_press = f
        end
    end

    if widget.key_up then
        local f = function(connector, mods, keysym, scancode)
            widget:key_up(mods, keysym, scancode)
        end
        for _, connector in ipairs(targets) do
            connector.on_key_up = f
        end
    end

    if widget.text_input then
        local f = function(connector, text)
            widget:text_input(text)
        end
        for _, connector in ipairs(targets) do
            connector.on_text_input = f
        end
    end

    local first_connector = targets[1]

    if widget.resize then
        -- First connector is leading for widget size
        first_connector.on_resize = function(connector)
            if connector.pixel_width then
                widget:resize(connector.pixel_width, connector.pixel_height)
            else
                widget:resize(connector.width, connector.height)
            end
            if widget.redraw then
                widget:redraw()
            end
            for _, c in ipairs(targets) do
                c.card = widget.card
            end
        end
    end

    if count > 1 then
        widget.on_update = function(widget, rect)
            for _, connector in ipairs(targets) do
                connector:redraw(rect)
            end
        end
    else
        widget.on_update = function(widget, rect)
            first_connector:redraw(rect)
        end
    end
end


local function widget_base()
    local wdg = { visible = true }

    wdg.resize = function(self, width, height)
        if not self.card or self.card.width ~= width or self.card.height ~= height then
            self.width = width
            self.height = height
            if width > 0 and height > 0 then
                self:redraw(true)
            else
                self.card = nil
            end
        end
    end

    wdg.set_visible = function(self, visible)
        assert(visible == true or visible == false, 'visible must be a boolean')
        if self.visible ~= visible then
            self.visible = visible
            if self.card then
                self:redraw()
            end
        end
    end

    return wdg
end


local function create_color_rect(color, callback)
    local wdg = widget_base()

    wdg.color = color or deck:Colour { r = 20, g = 20, b = 20, a = 180 }
    wdg.expand = true
    wdg.on_click = callback

    wdg.mouse_button = function(self, x, y, button, pressed)
        if pressed and self.on_click then
            self:on_click(x, y, button, pressed)
        end
    end

    wdg.redraw = function(self, force)
        if not self.card or force then
            self.card = deck:Card(self.width, self.height)
            self.card:clear(self.color)
        end
        if self.on_update then
            self:on_update()
        end
    end

    return wdg
end


-- Banged my keyboard for some unique integers
local BUTTON_NORMAL = 68134
local BUTTON_PRESSED = 912087
local BUTTON_HOVERED = 10955
local BUTTON_DISABLED = 33123

local function create_button(text, callback, initial_enabled)
    local btn = widget_base()

    btn.bgcolor = deck:Colour 'Blue'
    btn.text = text
    btn.font = default_font
    btn.callback = callback
    btn.width = 400
    btn.height = 100

    if initial_enabled == false then
        btn._internal_state = BUTTON_DISABLED
    else
        btn._internal_state = BUTTON_NORMAL
    end

    btn._update_state = function(self, newstate)
        --assert(newstate == BUTTON_NORMAL or newstate == BUTTON_PRESSED or newstate == BUTTON_HOVERED or newstate == BUTTON_DISABLED)
        if self._internal_state ~= newstate then
            self._internal_state = newstate
            if self.card then
                self:redraw()
            end
        end
    end

    btn.mouse_button = function(self, x, y, button, pressed)
        if button == 1 and self._internal_state ~= BUTTON_DISABLED then
            self._is_pressed = pressed
            if pressed then
                self:_update_state(BUTTON_PRESSED)
            elseif self._internal_state == BUTTON_PRESSED then
                self:_update_state(BUTTON_HOVERED)
                if self.callback then
                    self:callback()
                else
                    logger(logger.WARNING, 'Button \'', self.text, '\' does not have a callback function')
                end
            else
                self:_update_state(BUTTON_NORMAL)
            end
        end
    end

    btn.mouse_entered = function(self)
        if self._internal_state ~= BUTTON_DISABLED then
            if self._is_pressed then
                self:_update_state(BUTTON_PRESSED)
            else
                self:_update_state(BUTTON_HOVERED)
            end
        end
    end

    btn.mouse_left = function(self)
        if self._internal_state ~= BUTTON_DISABLED then
            self:_update_state(BUTTON_NORMAL)
        end
    end

    btn.get_preferred_size = function(self)
        return font_preferred_size(self.font, self.text)
    end

    btn.redraw = function(self, force)
        if self.disabled or self.enabled == false then
            self._internal_state = BUTTON_DISABLED
            self._is_pressed = false
            self.enabled = nil
            self.disabled = nil
        end

        if not self.normal or force then
            self.normal = deck:Card(self.width, self.height)
            self.normal:clear(self.bgcolor)
            if self.text and self.text ~= '' then
                local txt = self.font:render(self.text, self.width - 10)
                self.normal:blit(txt, txt:centered(self.normal))
            end
            self.hover = nil
            self.pressed = nil
            self.grayed = nil
        end

        if self._internal_state == BUTTON_PRESSED then
            if not self.pressed then
                self.pressed = self.normal.dup:darken(40)
            end
            self.card = self.pressed
        elseif self._internal_state == BUTTON_HOVERED then
            if not self.hover then
                self.hover = self.normal.dup:lighten(40)
            end
            self.card = self.hover
        elseif self._internal_state == BUTTON_DISABLED then
            if not self.grayed then
                if not self.pressed then
                    self.pressed = self.normal.dup:darken(40)
                end
                self.grayed = self.pressed.dup:desaturate()
            end
            self.card = self.grayed
        else
            self.card = self.normal
        end

        if self.on_update then
            self:on_update()
        end
    end

    btn.set_enabled = function(self, enabled)
        if enabled then
            self:enable()
        else
            self:disable()
        end
    end

    btn.disable = function(self)
        self._is_pressed = false
        self:_update_state(BUTTON_DISABLED)
    end

    btn.enable = function(self)
        self:_update_state(BUTTON_NORMAL)
    end

    return btn
end


local function create_label(text, fgcolor, bgcolor)
    local lbl = widget_base()

    lbl.bgcolor = bgcolor and bgcolor or deck:Colour 'Transparent'
    lbl.fgcolor = fgcolor and fgcolor or deck:Colour 'White'
    lbl.alignment = ALIGN_LEFT
    lbl.text = text
    lbl.font = default_font
    lbl.width = 400
    lbl.height = 100
    lbl.wordwrap = true

    lbl.get_preferred_size = function(self)
        return font_preferred_size(self.font, self.text)
    end

    lbl.redraw = function(self, force)
        if not self.card or force then
            self.card = deck:Card(self.width, self.height)
            self.card:clear(self.bgcolor)

            if self.text and self.text ~= '' then
                local txt
                if self.wordwrap then
                    txt = self.font:render(self.text, self.width - 10, self.alignment, self.fgcolor)
                else
                    txt = self.font:render(self.text, self.fgcolor)
                end

                local pos = txt:centered(self.card)
                if self.alignment == ALIGN_LEFT then
                    pos.x = 5
                elseif self.alignment == ALIGN_RIGHT then
                    pos.x = self.card.width - 5 - txt.width
                end

                self.card:blit(txt, pos)
            end
        end

        if self.on_update then
            self:on_update()
        end
    end

    lbl.set_text = function(self, text)
        if self.text ~= text then
            self.text = text
            if self.card then
                self:redraw(true)
            end
        end
    end

    lbl.set_fgcolor = function(self, fgcolor)
        if self.fgcolor ~= fgcolor then
            self.fgcolor = fgcolor
            if self.card then
                self:redraw(true)
            end
        end
    end

    lbl.set_bgcolor = function(self, bgcolor)
        if self.bgcolor ~= bgcolor then
            self.bgcolor = bgcolor
            if self.card then
                self:redraw(true)
            end
        end
    end

    lbl.set_alignment = function(self, alignment)
        assert(alignment == ALIGN_LEFT or alignment == ALIGN_CENTER or alignment == ALIGN_RIGHT)
        if self.alignment ~= alignment then
            self.alignment = alignment
            if self.card then
                self:redraw(true)
            end
        end
    end

    lbl.set_wordwrap = function(self, wordwrap)
        if self.wordwrap ~= wordwrap then
            self.wordwrap = wordwrap
            if self.card then
                self:redraw(true)
            end
        end
    end

    return lbl
end

local INPUT_NORMAL = 78134
local INPUT_FOCUSED = 812087
local INPUT_HOVERED = 20955
local INPUT_DISABLED = 43123

local function create_input_field(initial_text)
    local inp = widget_base()

    inp.bgcolor = deck:Colour 'Dimgray'
    inp.fgcolor = deck:Colour 'White'
    inp.text = initial_text
    inp.font = default_font
    inp.width = 400
    inp.height = 100
    inp._internal_state = INPUT_NORMAL

    inp.get_preferred_size = function(self)
        local min_size = self.input_length or 1
        local dummy = string.rep('F', min_size)
        return font_preferred_size(self.font, dummy)
    end

    inp._update_state = function(self, newstate)
        if self._internal_state ~= newstate then
            local was_focused = self._internal_state == INPUT_FOCUSED

            if was_focused and self.numerical then
                local real_value = tonumber(self.text)
                local real_text
                if not real_value then
                    real_text = '0'
                else
                    real_text = tostring(real_value)
                end
                self:set_text(real_text)
            end

            if newstate == INPUT_NORMAL and self._hovered then
                newstate = INPUT_HOVERED
            end

            self._internal_state = newstate
            if self.card then
                self:redraw()
            end

            if was_focused and self.on_blur then
                self:on_blur()
            end
        end
    end

    inp.blur = function(self)
        if self._internal_state == INPUT_FOCUSED then
            self:_update_state(INPUT_NORMAL)
        end
    end

    inp.mouse_entered = function(self)
        self._hovered = true
        if self._internal_state == INPUT_NORMAL then
            self:_update_state(INPUT_HOVERED)
        end
    end

    inp.mouse_left = function(self)
        self._hovered = false
        if self._internal_state == INPUT_HOVERED then
            self:_update_state(INPUT_NORMAL)
        end
    end

    inp.mouse_button = function(self, x, y, button, pressed)
        if pressed and self._internal_state == INPUT_HOVERED then
            self:_update_state(INPUT_FOCUSED)
        end
    end

    inp.key_press = function(self, mods, keysym, scancode)
        if self._internal_state == INPUT_FOCUSED then
            if keysym == 8 then -- backspace
                local new_text = string.sub(self.text, 1, -2)
                self:set_text(new_text)
            elseif keysym == 13 or keysym == 1073741912 then -- enter or KP enter
                self:_update_state(INPUT_NORMAL)
            end
        end
    end

    inp.text_input = function(self, text)
        if self._internal_state == INPUT_FOCUSED then
            local new_text = self.text .. text
            self:set_text(new_text)
        end
    end

    inp.redraw = function(self, force)
        if not self._txt or force then
            if self.text and self.text ~= '' then
                self._txt = self.font:render(self.text, self.fgcolor)
            else
                self._txt = nil
            end
        end
        if not self.card or force then
            self.card = deck:Card(self.width, self.height)
        end

        local bgcolor = self.bgcolor.dup
        if self._internal_state == INPUT_DISABLED then
            bgcolor:darken()
        elseif self._internal_state == INPUT_HOVERED then
            bgcolor:lighten(0.1)
        elseif self._internal_state == INPUT_FOCUSED then
            bgcolor:lighten(0.3)
        end

        self.card:clear(bgcolor)
        if self._txt then
            local pos = self._txt:centered(self.card)
            pos.x = 5
            self.card:blit(self._txt, pos)
        end

        if self.on_update then
            self:on_update()
        end
    end

    inp.set_text = function(self, text)
        if self.input_length and string.len(text) > self.input_length then
            text = string.sub(text, 1, self.input_length)
        end
        if self.text ~= text then
            if self.numerical and text ~= '' and text ~= '-' then
                local nr = tonumber(text)
                if nr == nil then
                    return
                end
                text = tostring(nr)
            end
            if self.validate_text then
                local retval = self:validate_text(text)
                if retval == false then
                    return
                end
                if type(retval) == 'string' then
                    text = retval
                end
            end
            self.text = text
            self._txt = nil
            if self.card then
                self:redraw()
            end
            if self.on_text_changed then
                self:on_text_changed(text)
            end
        end
    end

    inp.set_fgcolor = function(self, fgcolor)
        if self.fgcolor ~= fgcolor then
            self.fgcolor = fgcolor
            self._txt = nil
            if self.card then
                self:redraw()
            end
        end
    end

    inp.set_bgcolor = function(self, bgcolor)
        if self.bgcolor ~= bgcolor then
            self.bgcolor = bgcolor
            if self.card then
                self:redraw()
            end
        end
    end

    inp.set_enabled = function(self, enabled)
        if enabled then
            self:enable()
        else
            self:disable()
        end
    end

    inp.disable = function(self)
        self:_update_state(INPUT_DISABLED)
    end

    inp.enable = function(self)
        self:_update_state(INPUT_NORMAL)
    end

    return inp
end


local exports = {}

exports.default_font = default_font

exports.default_container = default_container
exports.create_window_manager = create_window_manager
exports.create_grid = create_grid
exports.create_border = create_border
exports.create_vbox = create_vbox
exports.create_hbox = create_hbox
exports.connect = connect
exports.disconnect = disconnect

exports.widget_base = widget_base
exports.create_color_rect = create_color_rect
exports.create_button = create_button
exports.create_label = create_label
exports.create_input_field = create_input_field

return exports
