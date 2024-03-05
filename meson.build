project(
    'deck-assistant',
    'c',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0',
    license_files: ['LICENSE'],
    default_options: ['cpp_std=c++20'],
)

if get_option('hidapi').allowed()
    hidapi = dependency(
        'hidapi-hidraw',
        disabler: false,
        required: false,
    )

    if not hidapi.found()
        hidapi = dependency(
            'hidapi-libusb',
            disabler: false,
            required: get_option('hidapi'),
        )
    endif
else
    hidapi = dependency(
        'hidapi-hidraw',
        disabler: false,
        required: get_option('hidapi'),
    )
endif

lua = dependency(
    'luajit',
    version: '>=2.1.0',
    required: get_option('luajit'),
)

if not lua.found()
    lua = dependency(
        'lua5.1',
        version: '>=5.1.5',
        required: true,
    )
endif

sdl = dependency(
    'sdl2',
    disabler: false,
    required: get_option('sdl'),
)

sdl_image = dependency(
    'SDL2_image',
    disabler: false,
    required: get_option('sdl-image'),
)

sdl_ttf = dependency(
    'SDL2_ttf',
    disabler: false,
    required: get_option('sdl-ttf'),
)

catch2 = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    disabler: true,
    required: get_option('test'),
)

summary(
    {
        'hidapi': get_option('hidapi'),
        'luajit': get_option('luajit'),
        'sdl': get_option('sdl'),
        'sdl-image': get_option('sdl-image'),
        'sdl-ttf': get_option('sdl-ttf'),
        'test': get_option('test'),
    },
    section: 'Features',
)

summary(
    {
        catch2.name(): catch2,
        hidapi.name(): hidapi,
        lua.name(): lua,
        sdl.name(): sdl,
        sdl_image.name(): sdl_image,
        sdl_ttf.name(): sdl_ttf,
    },
    section: 'Runtime dependencies',
)

buildflags = []
if hidapi.found()
    buildflags += '-DHAVE_HIDAPI'
endif
if sdl.found()
    buildflags += '-DHAVE_SDL'
endif
if sdl_image.found()
    buildflags += '-DHAVE_SDL_IMAGE'
endif
if sdl_ttf.found()
    buildflags += '-DHAVE_SDL_TTF'
endif

subdir('src')
