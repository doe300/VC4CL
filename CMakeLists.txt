cmake_minimum_required (VERSION 3.1)
find_package(PkgConfig)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

####
# General configuration
####
option(INCLUDE_COMPILER "Includes the VC4C compiler" ON)
option(BUILD_TESTING "Build testing program" OFF)
option(BUILD_ICD "Build with support for ICD" ON)
option(IMAGE_SUPPORT "Experimental image support" OFF)
# Option whether to use register-poking (faster) or mailbox-calls (synchronized system-wide) to run kernels
option(REGISTER_POKE_KERNELS "Use the faster way of register to run kernels" ON)
# Option to enable/disable test-program
option(BUILD_DEBUG "Build with debugging symbols. Otherwise build for performance" OFF)
# Option to enable/disable cross compilation
option(CROSS_COMPILE "Cross compile for Raspbian" OFF)

if(CROSS_COMPILE)
	if(NOT CROSS_COMPILER_PATH)
		set(CROSS_COMPILER_PATH "/opt/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64")
	endif()
	message(STATUS "Cross compiling for Raspbian with compiler: ${CROSS_COMPILER_PATH}")
	#Set Cross compiler
	SET(CMAKE_SYSTEM_NAME 		"Linux")
	SET(CMAKE_C_COMPILER   		"${CROSS_COMPILER_PATH}/bin/arm-linux-gnueabihf-gcc")
	SET(CMAKE_CXX_COMPILER 		"${CROSS_COMPILER_PATH}/bin/arm-linux-gnueabihf-g++")
	SET(CMAKE_FIND_ROOT_PATH  	"${CROSS_COMPILER_PATH}/arm-linux-gnueabihf")
endif()

project (VC4CL VERSION 0.4)
add_definitions(-DVC4CL_LIBRARY_VERSION="${PROJECT_VERSION}")

# append usage of C++ to compiler flags, also optimize for speed and enable all warnings
if(BUILD_DEBUG)
	LIST(APPEND CMAKE_C_FLAGS "-std=gnu11 -g3 -Wall -Wextra -Wold-style-cast -Wno-unused-parameter -Wno-ignored-attributes -fno-strict-aliasing -pthread")
	LIST(APPEND CMAKE_CXX_FLAGS "-std=c++0x -g3 -Wall -Wextra -Wold-style-cast -Wno-unused-parameter -Wno-ignored-attributes -fno-strict-aliasing -pthread")
	SET(CMAKE_BUILD_TYPE 		Debug)
	add_definitions(-DDEBUG_MODE=1)
else()
	LIST(APPEND CMAKE_C_FLAGS "-std=gnu11 -O3 -Wall -Wextra -Wold-style-cast -Wno-unused-parameter -Wno-ignored-attributes -fno-strict-aliasing -pthread")
	LIST(APPEND CMAKE_CXX_FLAGS "-std=c++0x -g3 -Wall -Wextra -Wold-style-cast -Wno-unused-parameter -Wno-ignored-attributes -fno-strict-aliasing -pthread")
	SET(CMAKE_BUILD_TYPE 		Release)
endif()

# Enable sanitizers (for now only for host pc)
# Raspberry Pi throws ASAN error on linking shared libraries (in ld)
if(BUILD_DEBUG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "6.0.0")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -fsanitize=undefined ")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdelete-null-pointer-checks -Wnull-dereference -Wuninitialized -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format -Wsuggest-override -Wconversion -Wzero-as-null-pointer-constant")
endif()

####
# Dependencies
####

# Check for OpenCL headers
find_package(OpenCL REQUIRED)
message(STATUS "Found headers for OpenCL up to version ${OpenCL_VERSION_STRING} located in ${OpenCL_INCLUDE_DIRS}")
include_directories(${OpenCL_INCLUDE_DIRS})

if(INCLUDE_COMPILER)
	if(NOT VC4C_HEADER_PATH)
		find_file(VC4C_HEADER_PATH_FOUND "VC4C.h" HINTS "${PROJECT_SOURCE_DIR}/lib/vc4c/include" "${PROJECT_SOURCE_DIR}/../VC4C/include")
		if(VC4C_HEADER_PATH_FOUND)
			set(VC4C_HEADER_PATH "${VC4C_HEADER_PATH_FOUND}")
		endif()
	endif()
	if(NOT VC4CC_LIBRARY)
		find_library(VC4CC_LIBRARY NAMES VC4CC libVC4CC HINTS "${PROJECT_SOURCE_DIR}/lib/vc4c/build" "${PROJECT_SOURCE_DIR}/../VC4C/build")
	endif()
	if(VC4CC_LIBRARY)
		message(STATUS "VC4C library found: ${VC4CC_LIBRARY}")
	else()
		message(STATUS "No VC4C compiler library found!")
	endif()
	if(VC4C_HEADER_PATH)
		message(STATUS "VC4C compiler header found at: ${VC4C_HEADER_PATH}")
		add_definitions(-DCOMPILER_HEADER="${VC4C_HEADER_PATH}")
		add_definitions(-DHAS_COMPILER=1)
	
	else()
		message(STATUS "No VC4C compiler header found!")
		add_definitions(-DHAS_COMPILER=0)
	endif()
endif()

# Enable Khronos ICD loader
if(BUILD_ICD)
	# To be exact, this enables the methods for the cl_khr_icd extension to be exported as publicly accessible
	add_definitions(-Dcl_khr_icd=1)
	add_definitions(-Duse_cl_khr_icd=1)
	message(STATUS "Building with ICD support")
	
	if(NOT CROSS_COMPILE)
		# accoding to pocl, pkg-config doesn't work with cross-compiling
		pkg_search_module(OCL_ICD REQUIRED ocl-icd>=1.3)
		message(STATUS "Found Khronos ICD Loader in version ${OCL_ICD_VERSION} in ${OCL_ICD_LIBDIR}")
	endif()
endif()

# Enable experimental image support
if(IMAGE_SUPPORT)
	add_definitions(-DIMAGE_SUPPORT=1)
	message(STATUS "Building with experimental image support")
endif() 

# Enable register poking for kernels
if(REGISTER_POKE_KERNELS)
	add_definitions(-DREGISTER_POKE_KERNELS=1)
	message(STATUS "Enabling register-poking to run kernels")
endif()

####
# Main files
####
#build all from ./src into ./build
add_subdirectory(./src build)
add_subdirectory(./tools build/tools)

if (BUILD_TESTING)
	if(NOT TARGET cpptest-lite)
		# only build cpptest-lite, if it doesn't exist yet (e.g. built by VC4CC)
		ExternalProject_Add(cpptest-lite-project
			PREFIX 				${CMAKE_BINARY_DIR}/build/cpptest-lite
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
	add_subdirectory(test build/test)
endif (BUILD_TESTING)

##
# Installation targets
##
# Adds custom uninstall command
add_custom_target(uninstall_vc4cl "${CMAKE_COMMAND}" -P "cmake_uninstall.cmake")
