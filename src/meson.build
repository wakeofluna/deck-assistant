sources = files(
    'application.cpp',
    'connector_elgato_streamdeck.cpp',
    'deck_card.cpp',
    'deck_component.cpp',
    'deck_connector.cpp',
    'deck_connector_container.cpp',
    'deck_font.cpp',
    'deck_font_container.cpp',
    'deck_logger.cpp',
    'deck_module.cpp',
    'deck_text.cpp',
    'i_connector.cpp',
    'lua_helpers.cpp',
)

slib = static_library(
    'deck-assistant',
    sources: sources,
    dependencies: [lua, hidapi, sdl, sdl_image, sdl_ttf],
    cpp_args: ['-fno-exceptions'] + buildflags,
)

executable(
    'deck',
    sources: ['main.cpp'],
    link_with: [slib],
    cpp_args: ['-fno-exceptions'],
)
