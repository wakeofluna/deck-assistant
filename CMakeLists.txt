cmake_minimum_required(VERSION 3.25)

project ( deck CXX )

#############################
# USER CONFIGURABLE OPTIONS #
#############################

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build")

####################################
# END OF USER CONFIGURABLE OPTIONS #
####################################

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "-pipe -Wall -march=native -fvisibility=hidden -fno-rtti")
	#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage")
endif()

include(FindPkgConfig)
pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_image IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_ttf IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(SDL2_net IMPORTED_TARGET SDL2_net)
pkg_search_module(lua IMPORTED_TARGET luajit lua5.1)
pkg_search_module(ssl IMPORTED_TARGET gnutls openssl)
pkg_check_modules(vncserver IMPORTED_TARGET libvncserver)

option(BUILD_SHARED_LIBS OFF)
set(BUILD_SHARED_LIBS OFF)
option(SDL_SHARED "" OFF)
option(SDL_STATIC "" ON)
option(SDL_HIDAPI "" ON)
option(SDL_DISABLE_INSTALL "" ON)
option(SDL2_DISABLE_INSTALL "" ON)

include(FetchContent)
include(ExternalProject)

if (NOT sdl2_FOUND)
	FetchContent_Declare(
		SDL2
		SYSTEM
		EXCLUDE_FROM_ALL
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG release-2.32.8
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
		FIND_PACKAGE_ARGS NAMES SDL2
	)
	FetchContent_MakeAvailable(SDL2)
else()
	add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

if (NOT SDL2_image_FOUND)
	option(SDL2IMAGE_AVIF "" NO)
	option(SDL2IMAGE_LBM "" NO)
	option(SDL2IMAGE_PNM "" NO)
	option(SDL2IMAGE_QOI "" NO)
	option(SDL2IMAGE_TIF "" NO)
	option(SDL2IMAGE_WEBP "" NO)
	option(SDL2IMAGE_XCF "" NO)
	option(SDL2IMAGE_XV "" NO)
	FetchContent_Declare(
		SDL2_image
		SYSTEM
		EXCLUDE_FROM_ALL
		GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
		GIT_TAG release-2.8.8
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
		FIND_PACKAGE_ARGS NAMES SDL2_image
	)
	FetchContent_MakeAvailable(SDL2_image)
else()
	add_library(SDL2_image ALIAS PkgConfig::SDL2_image)
endif()

if (NOT SDL2_ttf_FOUND)
	FetchContent_Declare(
		SDL2_ttf
		SYSTEM
		EXCLUDE_FROM_ALL
		GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
		GIT_TAG release-2.24.0
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
		FIND_PACKAGE_ARGS NAMES SDL2_ttf
	)
	FetchContent_MakeAvailable(SDL2_ttf)
else()
	add_library(SDL2_ttf ALIAS PkgConfig::SDL2_ttf)
endif()

if (NOT SDL2_net_FOUND)
	FetchContent_Declare(
		SDL2_net
		SYSTEM
		EXCLUDE_FROM_ALL
		GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
		GIT_TAG release-2.2.0
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
		FIND_PACKAGE_ARGS NAMES SDL2_net
	)
	FetchContent_MakeAvailable(SDL2_net)
else()
	add_library(SDL2_net ALIAS PkgConfig::SDL2_net)
endif()

if (NOT lua_FOUND)
	FetchContent_Declare(
		lua
		SYSTEM
		EXCLUDE_FROM_ALL
		GIT_REPOSITORY https://luajit.org/git/luajit.git
		GIT_TAG origin/v2.1
		#GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
	)
	FetchContent_MakeAvailable(lua)

	if (MSVC)
		set(LUA_BUILD_CMD ".\\msvcbuild.bat" "static")
		set(LUA_LIBNAME "lua51.lib")
	else()
		find_program(MAKE_EXE NAMES gmake nmake make)
		set(LUA_BUILD_CMD ${MAKE_EXE})
		set(LUA_LIBNAME "libluajit.a")
	endif()

	FetchContent_GetProperties(lua SOURCE_DIR LUA_SRC_DIR)
	set(LUA_SRC_DIR ${LUA_SRC_DIR}/src)
	set(LUA_LIBNAME ${LUA_SRC_DIR}/${LUA_LIBNAME})

	add_custom_command(
		OUTPUT ${LUA_LIBNAME}
		COMMAND ${LUA_BUILD_CMD}
		WORKING_DIRECTORY ${LUA_SRC_DIR}
		VERBATIM
	)

	add_custom_target(
		luajit-build
		DEPENDS ${LUA_LIBNAME}
	)

	add_library(lua STATIC IMPORTED GLOBAL)
	set_property(TARGET lua PROPERTY IMPORTED_LOCATION ${LUA_LIBNAME})
	target_include_directories(lua INTERFACE ${LUA_SRC_DIR})
	target_compile_definitions(lua INTERFACE HAVE_LUAJIT)
	add_dependencies(lua luajit-build)
else()
	if (lua_MODULE_NAME STREQUAL "luajit")
		target_compile_definitions(PkgConfig::lua INTERFACE HAVE_LUAJIT)
	endif()
	add_library(lua ALIAS PkgConfig::lua)
endif()

if (NOT ssl_FOUND)
	if (WIN32)
		FetchContent_Declare(
			openssl
			SYSTEM
			EXCLUDE_FROM_ALL
			URL https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-3.5.0.zip
			URL_HASH SHA256=9AC6B98D947E558E6ADBBC9CC9EAF504B3540BAB90767CF71E3582A14F75AFBD
		)
		FetchContent_MakeAvailable(openssl)
		FetchContent_GetProperties(openssl SOURCE_DIR OPENSSL_SRC_DIR)
		add_library(libssl SHARED IMPORTED GLOBAL)
		set_target_properties(libssl PROPERTIES
			IMPORTED_IMPLIB ${OPENSSL_SRC_DIR}/x64/lib/libssl.lib
			IMPORTED_LOCATION ${OPENSSL_SRC_DIR}/x64/bin/libssl-3-x64.dll
		)
		add_library(libcrypto SHARED IMPORTED GLOBAL)
		set_target_properties(libcrypto PROPERTIES
			IMPORTED_IMPLIB ${OPENSSL_SRC_DIR}/x64/lib/libcrypto.lib
			IMPORTED_LOCATION ${OPENSSL_SRC_DIR}/x64/bin/libcrypto-3-x64.dll
		)
		add_library(ssl INTERFACE)
		target_link_libraries(ssl INTERFACE libssl libcrypto crypt32.lib)
		target_include_directories(ssl INTERFACE ${OPENSSL_SRC_DIR}/x64/include)
		target_compile_definitions(ssl INTERFACE HAVE_OPENSSL OPENSSL_NO_DEPRECATED)
		install(TARGETS libssl libcrypto RUNTIME DESTINATION bin)
	endif ()
else()
	if (ssl_MODULE_NAME STREQUAL "gnutls")
		target_compile_definitions(PkgConfig::ssl INTERFACE HAVE_GNUTLS)
		option(WITH_GNUTLS ON)
	elseif (ssl_MODULE_NAME STREQUAL "openssl")
		target_compile_definitions(PkgConfig::ssl INTERFACE HAVE_OPENSSL OPENSSL_NO_DEPRECATED)
		option(WITH_OPENSSL ON)
	endif()
	add_library(ssl ALIAS PkgConfig::ssl)
endif()

if (NOT vncserver_FOUND)
	option(WITH_EXAMPLES "" OFF)
	option(WITH_TESTS "" OFF)
	FetchContent_Declare(
		vncserver
		SYSTEM
		EXCLUDE_FROM_ALL
		URL https://github.com/LibVNC/libvncserver/archive/refs/tags/LibVNCServer-0.9.15.tar.gz
		URL_HASH SHA256=62352c7795e231dfce044beb96156065a05a05c974e5de9e023d688d8ff675d7
		FIND_PACKAGE_ARGS NAMES libvncserver
	)
	FetchContent_MakeAvailable(vncserver)
	target_compile_definitions(vncserver INTERFACE HAVE_VNC)
else()
	target_compile_definitions(PkgConfig::vncserver INTERFACE HAVE_VNC)
	add_library(vncserver ALIAS PkgConfig::vncserver)
endif()

find_package(Catch2 QUIET)
if (NOT Catch2_DIR STREQUAL "Catch2_DIR-NOTFOUND")
	enable_testing()
	include(Catch)
	macro(catchtest name)
		add_executable(test_${name} ${ARGN})
		target_link_libraries(test_${name} PRIVATE Catch2::Catch2WithMain ${name})
		set_target_properties(test_${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
		target_compile_definitions(test_${name} PRIVATE TESTING)
		catch_discover_tests(test_${name})
	endmacro()
else()
	macro(catchtest name)
	endmacro()
endif()

add_subdirectory(tools)
add_subdirectory(scripts)
add_subdirectory(src)
