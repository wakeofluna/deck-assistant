set(SOURCES
    application.cpp
    connector_base.cpp
    connector_elgato_streamdeck.cpp
    connector_http.cpp
    connector_server_socket.cpp
    connector_server_socket_client.cpp
    connector_vnc.cpp
    connector_websocket.cpp
    connector_window.cpp
    deck_card.cpp
    deck_colour.cpp
    deck_connector_container.cpp
    deck_connector_factory.cpp
    deck_font.cpp
    deck_logger.cpp
    deck_module.cpp
    deck_promise.cpp
    deck_promise_list.cpp
    deck_rectangle.cpp
    deck_rectangle_list.cpp
    deck_util.cpp
    lua_class.cpp
    lua_helpers.cpp
    util_blob.cpp
    util_colour.cpp
    util_paths.cpp
    util_socket.cpp
    util_text.cpp
    util_tls_session.cpp
    util_url.cpp
)

set(TEST_SOURCES
    deck_rectangle_test.cpp
    lua_class_test.cpp
    lua_helpers_test.cpp
    test_utils_test.cpp
    util_blob_test.cpp
    util_text_test.cpp
    util_url_test.cpp
)

set(BUILTIN_SOURCES
    builtins/Vera.ttf
    builtins/deck_assistant.svg
    builtins/oauth2_callback.html
)

###################################################

foreach(BUILTIN_SRC ${BUILTIN_SOURCES})
    embed(${BUILTIN_SRC})
endforeach()
add_library(builtins STATIC ${EMBED_SOURCES})

add_library(decklib STATIC ${SOURCES})
target_link_libraries(decklib PUBLIC
    SDL2::SDL2
    SDL2_image
    SDL2_net
    SDL2_ttf
    lua
    ssl
    vnc
    builtins
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(decklib PRIVATE -fno-exceptions)
endif()

catchtest(decklib ${TEST_SOURCES})

add_executable(${PROJECT_NAME} main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE 1)
target_link_libraries(${PROJECT_NAME} decklib)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)