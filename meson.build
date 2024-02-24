project(
    'deck-assistant',
    'c',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0',
    license_files: ['LICENSE'],
    default_options: ['cpp_std=c++20'],
)

hidapi = dependency(
    'hidapi-hidraw',
    disabler: true,
    required: false,
)

if not hidapi.found()
    hidapi = dependency(
        'hidapi-libusb',
        disabler: true,
        required: false,
    )
endif

feature_streamdeck = get_option(
    'streamdeck',
).require(hidapi.found(), error_message: 'requires hidapi')

lua = dependency(
    'lua5.4',
    version: '>=5.4.6',
    required: true,
)

sdl = dependency(
    'sdl2',
    required: true,
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
        'catch2': catch2,
        'hidapi': hidapi,
        'sdl': sdl,
        'sdl-image': sdl_image,
        'sdl-ttf': sdl_ttf,
    },
    section: 'Runtime dependencies',
)

summary(
    {
        'test': get_option('test'),
        'sdl-image': get_option('sdl-image'),
        'sdl-ttf': get_option('sdl-ttf'),
        'streamdeck': feature_streamdeck,
    },
    section: 'Features',
)

subdir('src')
