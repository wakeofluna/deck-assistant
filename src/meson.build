sources = files(
    'application.cpp',
    'deck_card.cpp',
    'deck_component.cpp',
    'deck_font.cpp',
    'deck_font_container.cpp',
    'deck_module.cpp',
    'deck_text.cpp',
    'lua_class.cpp',
)

flags = []

if sdl_image.found()
    flags += '-DHAVE_SDL_IMAGE'
endif

if sdl_ttf.found()
    flags += '-DHAVE_SDL_TTF'
endif

slib = static_library(
    'deck-assistant',
    sources: sources,
    dependencies: [lua, sdl, sdl_image, sdl_ttf],
    cpp_args: ['-fno-exceptions'] + flags,
)

executable(
    'deck',
    sources: ['main.cpp'],
    link_with: [slib],
    cpp_args: ['-fno-exceptions'],
)
