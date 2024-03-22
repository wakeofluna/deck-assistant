sources = files(
    'application.cpp',
    'builtins.cpp',
    'connector_base.cpp',
    'connector_elgato_streamdeck.cpp',
    'connector_window.cpp',
    'deck_card.cpp',
    'deck_colour.cpp',
    'deck_connector_container.cpp',
    'deck_connector_factory.cpp',
    'deck_font.cpp',
    'deck_logger.cpp',
    'deck_module.cpp',
    'deck_rectangle.cpp',
    'lua_class.cpp',
    'lua_helpers.cpp',
    'util_colour.cpp',
)

test_sources = files(
    'deck_rectangle_test.cpp',
    'lua_class_test.cpp',
    'lua_helpers_test.cpp',
    'test_utils_test.cpp',
)

slib = static_library(
    'deck-assistant',
    sources: sources,
    dependencies: [lua, sdl, sdl_image, sdl_ttf],
    cpp_args: ['-fno-exceptions'] + buildflags,
)

executable(
    'deck',
    sources: ['main.cpp'],
    link_with: [slib],
    cpp_args: ['-fno-exceptions'],
)

tests = executable(
    'testdeck',
    sources: test_sources,
    link_with: [slib],
    dependencies: [catch2, lua, sdl],
    cpp_args: buildflags,
)

test('testdeck', tests)
