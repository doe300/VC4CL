cmake_minimum_required (VERSION 3.1)
find_package(PkgConfig)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
include(CheckSymbolExists)

####
# General configuration
####
option(INCLUDE_COMPILER "Includes the VC4C compiler" ON)
option(BUILD_TESTING "Build testing program" OFF)
option(BUILD_ICD "Build with support for ICD" ON)
option(IMAGE_SUPPORT "Experimental image support" OFF)
# Option whether to use register-poking (faster) or mailbox-calls (synchronized system-wide) to run kernels
option(REGISTER_POKE_KERNELS "Use the faster way of register to run kernels" ON)
# Option to enable/disable cross compilation
option(CROSS_COMPILE "Cross compile for Raspbian" OFF)
# Option whether to emulate the hardware interface for testing purposes
option(MOCK_HAL "Emulate hardware interface" OFF)
# Option whether to enable code coverage analysis via gcov
option(ENABLE_COVERAGE "Enables collection of code coverage via gcov" OFF)
# Option whether to enable more compile-time checks
option(ADVANCED_CHECKS "Enable advanced compile-time checks" OFF)

if(CROSS_COMPILE)
	include(cmake/crosscompile.cmake)
	message(STATUS "Cross compiling for Raspbian with compiler: ${CMAKE_CXX_COMPILER}")
endif()

if(NOT BUILD_NUMBER)
	set(BUILD_NUMBER 9999)
endif()

project (VC4CL VERSION 0.4.${BUILD_NUMBER})

# Set C++ standard to C++14 without any extensions (e.g. GNU)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -rdynamic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# Set default build type to Debug
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Set optimization and warning flags for the build types
include(cmake/flags.cmake)

# clang-tidy
find_program(CLANG_TIDY NAMES clang-tidy clang-tidy-5.0 clang-tidy-6.0 clang-tidy-7 clang-tidy-8)
if(ADVANCED_CHECKS AND CLANG_TIDY)
    message(STATUS "Enabling clang-tidy compile time checks: ${CLANG_TIDY}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
endif()

####
# Dependencies
####

if(BUILD_OFFLINE)
	# A result of != 0 is an error, so disable updating
	set_property(DIRECTORY ${VC4C_SOURCE_DIR} PROPERTY EP_UPDATE_DISCONNECTED 1)
	message(WARNING "Building in off-line mode, some dependencies might not be up-to-date!")
else()
	set_property(DIRECTORY ${VC4C_SOURCE_DIR} PROPERTY EP_UPDATE_DISCONNECTED 0)
endif()

# Check for OpenCL headers (if not specified manually)
if(NOT OpenCL_INCLUDE_DIRS)
	find_package(OpenCL REQUIRED)
endif()
message(STATUS "Found headers for OpenCL up to version ${OpenCL_VERSION_STRING} located in ${OpenCL_INCLUDE_DIRS}")

# Check for and include the compiler binaries and header
include(cmake/vc4cc.cmake)

# Enable Khronos ICD loader
include(cmake/icd.cmake)

# Enable experimental image support
if(IMAGE_SUPPORT)
	message(STATUS "Building with experimental image support")
endif()

if(MOCK_HAL)
	if(NOT (INCLUDE_COMPILER AND VC4C_HEADER_PATH))
		message(SEND_ERROR "Cannot run in emulation mode without compiler available!")
	endif()
	# Disable ICD loading to support directly forcing the emulation runtime
	# (e.g. via LD_PRELOAD)
	set(BUILD_ICD OFF)
	# Disable register poking for emulation, since it is not yet supported
	set(REGISTER_POKE_KERNELS OFF)
endif()

# Enable register poking for kernels
if(REGISTER_POKE_KERNELS)
	message(STATUS "Enabling register-poking to run kernels")
endif()

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses)
if(NOT CURSES_FOUND)
	message(STATUS "NCurses not found on this machine, the V3D profiler will not be built")
endif()

####
# Additional configuration
####
include(cmake/sanitizers.cmake)

####
# Main files
####
#build all from ./src into ./build
add_subdirectory(src)
add_subdirectory(tools)

if (BUILD_TESTING)
	enable_testing()
	if(NOT TARGET cpptest-lite)
		# only build cpptest-lite, if it doesn't exist yet (e.g. built by VC4CC)
		ExternalProject_Add(cpptest-lite-project
			PREFIX 				${CMAKE_BINARY_DIR}/cpptest-lite
			SOURCE_DIR 			${CMAKE_SOURCE_DIR}/lib/cpptest-lite
			GIT_REPOSITORY		https://github.com/doe300/cpptest-lite.git
			UPDATE_COMMAND 		git pull -f https://github.com/doe300/cpptest-lite.git
			STEP_TARGETS 		build
	  		EXCLUDE_FROM_ALL	TRUE
	  		TIMEOUT 			30		#Timeout for downloads, in seconds
  		        CMAKE_ARGS
    		          -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    		          -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  		          -DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}

		)
	endif()
	add_subdirectory(test)
endif (BUILD_TESTING)

##
# Installation targets
##
include(cmake/installation.cmake)

include(cmake/packaging.cmake)
