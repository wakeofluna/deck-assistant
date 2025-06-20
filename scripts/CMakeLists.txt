if (WIN32)
    set(SCRIPT_DEST bin/scripts)
else()
    set(SCRIPT_DEST share/deck-assistant)
endif()

install(DIRECTORY .
    DESTINATION ${SCRIPT_DEST}
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    PATTERN "CMakeLists.txt" EXCLUDE
)