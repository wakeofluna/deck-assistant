sources = files(
    'application.cpp',
    'builtins.cpp',
    'connector_elgato_streamdeck.cpp',
    'deck_card.cpp',
    'deck_colour.cpp',
    'deck_connector.cpp',
    'deck_connector_container.cpp',
    'deck_formatter.cpp',
    'deck_image.cpp',
    'deck_logger.cpp',
    'deck_module.cpp',
    'deck_text.cpp',
    'i_connector.cpp',
    'lua_helpers.cpp',
    'util_colour.cpp',
)

test_sources = files(
    'lua_class_test.cpp',
    'lua_helpers_test.cpp',
    'test_utils_test.cpp',
)

slib = static_library(
    'deck-assistant',
    sources: sources,
    dependencies: [lua, sdl, sdl_image, sdl_ttf],
    cpp_args: ['-fno-exceptions'],
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
    dependencies: [catch2, lua],
)

test('testdeck', tests)
