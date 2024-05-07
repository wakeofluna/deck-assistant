local_xxd = executable(
    'local_xxd',
    sources: files('local_xxd.cpp'),
    install: false,
    native: true,
)

embedder = generator(
    local_xxd,
    arguments: ['@INPUT@', '@OUTPUT@', '@PLAINNAME@'],
    output: '@PLAINNAME@.cpp',
)
