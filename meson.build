project(
    'deck-assistant',
    'c',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0',
    license_files: ['LICENSE'],
    default_options: ['cpp_std=c++20'],
)

feature_hidapi = get_option('hidapi')
feature_luajit = get_option('luajit')
feature_sdl = get_option('sdl')
feature_sdl_image = get_option(
    'sdl-image',
).require(feature_sdl.allowed(), error_message: 'SDL_image requires SDL')
feature_sdl_ttf = get_option(
    'sdl-ttf',
).require(feature_sdl.allowed(), error_message: 'SDL_ttf requires SDL')
feature_test = get_option('test')

if feature_hidapi.allowed()
    hidapi = dependency(
        'hidapi-hidraw',
        disabler: false,
        required: false,
    )

    if not hidapi.found()
        hidapi = dependency(
            'hidapi-libusb',
            disabler: false,
            required: feature_hidapi,
        )
    endif
else
    hidapi = dependency(
        'hidapi-hidraw',
        disabler: false,
        required: feature_hidapi,
    )
endif

lua = dependency(
    'luajit',
    version: '>=2.1.0',
    required: feature_luajit,
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
    required: feature_sdl,
)

sdl_image = dependency(
    'SDL2_image',
    disabler: false,
    required: feature_sdl_image,
)

sdl_ttf = dependency(
    'SDL2_ttf',
    disabler: false,
    required: feature_sdl_ttf,
)

catch2 = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    disabler: true,
    required: feature_test,
)

summary(
    {
        'hidapi': feature_hidapi,
        'luajit': feature_luajit,
        'sdl': feature_sdl,
        'sdl-image': feature_sdl_image,
        'sdl-ttf': feature_sdl_ttf,
        'test': feature_test,
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
