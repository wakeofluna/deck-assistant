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
feature_ssl = get_option('ssl')
feature_vnc = get_option('vnc')

if host_machine.system() == 'windows'
    do_fallbacks = true
else
    do_fallbacks = false
endif

# MIT license
lua = dependency(
    'luajit',
    version: '>=2.1.0',
    required: feature_luajit,
)

if lua.found()
    buildflags += '-DHAVE_LUAJIT'
else
    # MIT license
    lua = dependency(
        'lua5.1',
        version: '>=5.1.5',
        required: false,
    )
endif

if not lua.found()
    lua = dependency(
        'luajit-2.1',
        required: true,
        allow_fallback: do_fallbacks,
    )
    buildflags += '-DHAVE_LUAJIT'
endif

# zlib license
sdl = dependency(
    'sdl2',
    disabler: false,
    required: true,
    allow_fallback: do_fallbacks,
    default_options: ['default_library=shared'],
)

# zlib license
sdl_image = dependency(
    'SDL2_image',
    disabler: false,
    required: true,
    allow_fallback: do_fallbacks,
    default_options: ['default_library=static'],
)

# zlib license
sdl_net = dependency(
    'SDL2_net',
    disabler: false,
    required: true,
    allow_fallback: do_fallbacks,
    default_options: ['default_library=static'],
)

# zlib license
sdl_ttf = dependency(
    'SDL2_ttf',
    disabler: false,
    required: true,
    allow_fallback: do_fallbacks,
    default_options: ['default_library=static'],
)

# LGPL-2.1 or later
ssl = dependency(
    'gnutls',
    disabler: false,
    required: feature_gnutls,
)

if ssl.found()
    buildflags += '-DHAVE_GNUTLS'
else
    # Apache License v2
    ssl = dependency(
        'openssl',
        disabler: false,
        required: feature_ssl,
        allow_fallback: do_fallbacks,
        default_options: ['default_library=shared'],
    )
    if ssl.found()
        buildflags += '-DHAVE_OPENSSL'
    endif
endif

# GPL-2.0 or later
vnc = dependency(
    'libvncserver',
    disabler: false,
    required: feature_vnc,
    allow_fallback: do_fallbacks,
    default_options: ['default_library=shared'],
)

if vnc.found()
    buildflags += '-DHAVE_VNC'
endif

# MIT license
catch2 = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    disabler: true,
    required: feature_test,
    allow_fallback: do_fallbacks,
)

summary(
    {
        'luajit': feature_luajit,
        'test': feature_test,
        'gnutls': feature_gnutls,
        'ssl': feature_ssl,
        'vnc': feature_vnc,
    },
    section: 'Features',
)

summary(
    {
        lua.name(): lua,
        sdl.name(): sdl,
        sdl_image.name(): sdl_image,
        sdl_net.name(): sdl_net,
        sdl_ttf.name(): sdl_ttf,
        ssl.name(): ssl,
        vnc.name(): vnc,
    },
    section: 'Runtime dependencies',
)

subdir('tools')
subdir('src')
