set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)
add_executable(xxd local_xxd.cpp)

macro(embed file)
    set(ITEM ${file})
    cmake_path(GET ITEM FILENAME FNAME)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ITEM}.cpp
        COMMAND xxd ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM} ${CMAKE_CURRENT_BINARY_DIR}/${ITEM}.cpp ${FNAME}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}
    )
    set(EMBED_SOURCES ${EMBED_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/${ITEM}.cpp)
endmacro()