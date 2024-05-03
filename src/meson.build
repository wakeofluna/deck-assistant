sources = files(
    'application.cpp',
    'builtins.cpp',
    'connector_base.cpp',
    'connector_elgato_streamdeck.cpp',
    'connector_vnc.cpp',
    'connector_websocket.cpp',
    'connector_window.cpp',
    'deck_card.cpp',
    'deck_colour.cpp',
    'deck_connector_container.cpp',
    'deck_connector_factory.cpp',
    'deck_font.cpp',
    'deck_logger.cpp',
    'deck_module.cpp',
    'deck_promise.cpp',
    'deck_promise_list.cpp',
    'deck_rectangle.cpp',
    'deck_rectangle_list.cpp',
    'deck_util.cpp',
    'lua_class.cpp',
    'lua_helpers.cpp',
    'util_blob.cpp',
    'util_colour.cpp',
    'util_paths.cpp',
    'util_socket.cpp',
    'util_text.cpp',
    'util_url.cpp',
)

test_sources = files(
    'deck_rectangle_test.cpp',
    'lua_class_test.cpp',
    'lua_helpers_test.cpp',
    'test_utils_test.cpp',
    'util_blob_test.cpp',
    'util_text_test.cpp',
    'util_url_test.cpp',
)

if build_machine.system() != 'windows'
    noexcept = ['-fno-exceptions']
else
    noexcept = []
endif


slib = static_library(
    'deck-assistant',
    sources: sources,
    dependencies: [lua, sdl, sdl_image, sdl_net, sdl_ttf, ssl, vnc],
    cpp_args: noexcept + buildflags,
)

executable(
    'deck',
    sources: ['main.cpp'],
    link_with: [slib],
    cpp_args: noexcept,
    install: true,
)

tests = executable(
    'testdeck',
    sources: test_sources,
    link_with: [slib],
    dependencies: [catch2, lua, sdl, ssl],
    cpp_args: buildflags,
)

test('testdeck', tests)
