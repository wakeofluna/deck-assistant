project(
    'deck-assistant',
    'c',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0',
    license_files: ['LICENSE'],
    default_options: ['cpp_std=c++20'],
)

buildflags = []
feature_luajit = get_option('luajit')
feature_test = get_option('test')
feature_gnutls = get_option('gnutls')

lua = dependency(
    'luajit',
    version: '>=2.1.0',
    required: feature_luajit,
)

if lua.found()
    buildflags += '-DHAVE_LUAJIT'
else
    lua = dependency(
        'lua5.1',
        version: '>=5.1.5',
        required: true,
    )
endif

sdl = dependency(
    'sdl2',
    disabler: false,
    required: true,
)

sdl_image = dependency(
    'SDL2_image',
    disabler: false,
    required: true,
)

sdl_ttf = dependency(
    'SDL2_ttf',
    disabler: false,
    required: true,
)

ssl = dependency(
    'gnutls',
    disabler: false,
    required: feature_gnutls,
)

if ssl.found()
    buildflags += '-DHAVE_GNUTLS'
endif

catch2 = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    disabler: true,
    required: feature_test,
)

summary(
    {
        'luajit': feature_luajit,
        'test': feature_test,
        'gnutls': feature_gnutls,
    },
    section: 'Features',
)

summary(
    {
        lua.name(): lua,
        sdl.name(): sdl,
        sdl_image.name(): sdl_image,
        sdl_ttf.name(): sdl_ttf,
        ssl.name(): ssl,
    },
    section: 'Runtime dependencies',
)

subdir('src')
