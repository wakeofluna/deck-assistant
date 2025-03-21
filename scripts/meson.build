scripts = files(
    'connector.oauth2.lua',
    'connector.obs.lua',
    'connector.ollama.lua',
    'connector.twitch.lua',
    'deck.widgets.lua',
)

install_data(
    scripts,
    install_dir: get_option('datadir') / 'deck-assistant',
    install_tag: 'bin',
    install_mode: 'rw-r--r--',
)
