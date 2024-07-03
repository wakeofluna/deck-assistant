# Deck Assistant

Deck Assistant is a program that creates control panels based on user-provided scripts. The application is named as a helper (assistant) to create user control panels (decks). Internally the graphic buffers are referred to as Cards, resulting in a control panel being a Deck of Cards.

> This application is still **WIP**. It is functional but there are no example scripts or documentation available. The Deck-API is currently not stable and subject to change. This README is also still a placeholder.

[![Maintenance](https://img.shields.io/badge/Maintained-yes-green.svg)](https://github.com/wakeofluna/deck-assistant)
[![GitHub](https://img.shields.io/github/license/wakeofluna/deck-assistant)](https://github.com/wakeofluna/deck-assistant/blob/master/LICENSE)
[![Documentation](https://img.shields.io/badge/Documentation-not_yet-red.svg?style=flat)](https://github.com/wakeofluna/deck-assistant)

# Overview

Control panels are defined using Lua 5.1. Using Lua allows users to make the control panel as simple or as complicated as is needed. In the application, the control panels are rendered using SDL.

For the control panel display, the user can use any combination of the following:

* A PC Desktop to show a full rendering of the control panel
* A VNC client (from e.g. a tablet) to create a touch screen control panel.
* An Elgato StreamDeck to show custom rendered buttons

Input from the control panels (mouse input, touch input, button input) is relayed to the script using callbacks. In the script, the user can perform any action that is required, such as:

* Modify the control panel (changing buttons, switching pages, writing text, ...)
* Send a command to a websocket (control OBS, VTube Studio, ...)
* Execute an application

Deck Assistant is tested for Windows and Linux, but should also run on iOS.

# Examples

To run example code, create a file called `deckfile.lua` and call the compiled program with this file as its first parameter.

## Import the DeckAssistant API into lua

```lua
local deck = require('deck')
local logger = require('deck.logger')
local util = require('deck.util')
local builtins = require('builtins')
```

## Create a window and link it to both a PC desktop and VNC output

```lua
local wm = builtins.create_window_manager()
vnc = deck:Connector('Vnc')
window = deck:Connector('Window', { width = 1000, height = 600 })
builtins.connect(wm, window, vnc)
```

## Connect to OBS

```lua
obs = deck:Connector('OBS', 'obs', { password = '...' })
```

## Add a bunch of buttons to the window

```lua
local grid = builtins.create_widget_grid(5, 4)
wm:add_child(grid)

local btn
btn = builtins.create_button('ToggleStream', function() obs:ToggleStream() end)
grid:add_child(btn, 0, 0)
btn = builtins.create_button('StartStream', function() obs:StartStream() end)
grid:add_child(btn, 1, 0)
btn = builtins.create_button('StopStream', function() obs:StopStream() end)
grid:add_child(btn, 2, 0)
btn = builtins.create_button('Scene 1', function() obs:SetScene('Scene') end)
grid:add_child(btn, 0, 1)
btn = builtins.create_button('Scene 2', function() obs:SetScene('Other Scene') end)
grid:add_child(btn, 1, 1)
```