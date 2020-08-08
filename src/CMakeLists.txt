add_library( VC4CL SHARED "")
target_compile_definitions(VC4CL PUBLIC -DVC4CL_LIBRARY_VERSION="${PROJECT_VERSION}")
if(OpenCL_FOUND)
	# OpenCL was included via the default find_package() and therefore is already installed
	target_include_directories(VC4CL SYSTEM PUBLIC ${OpenCL_INCLUDE_DIRS})
else()
	# support for custom OpenCL headers for build and system headers after installation
	get_filename_component(OPENCL_HEADERS ${OpenCL_INCLUDE_DIRS} ABSOLUTE ${CMAKE_BINARY_DIR})
	target_include_directories(VC4CL PUBLIC
	    $<BUILD_INTERFACE:${OPENCL_HEADERS}>
	    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
	)
endif()

# append usage of C++ to compiler flags, also optimize for speed and enable all warnings
target_compile_options(VC4CL PRIVATE ${VC4CL_ENABLED_WARNINGS})

# Add all sources
include(sources.list)

if(INCLUDE_COMPILER AND EXISTS "${VC4C_HEADER_PATH}")
	target_compile_definitions(VC4CL PUBLIC -DCOMPILER_HEADER="${VC4C_HEADER_PATH}" -DVC4C_TOOLS_HEADER="${VC4C_TOOLS_HEADER_PATH}" -DHAS_COMPILER=1)
	target_link_libraries(VC4CL ${VC4CC_LIBRARY} ${SYSROOT_LIBRARY_FLAGS})
endif()

if(MOCK_HAL)
	target_compile_definitions(VC4CL PRIVATE MOCK_HAL=1)
elseif(CROSS_COMPILE OR EXISTS "/opt/vc/include/bcm_host.h")
	find_library(BCMHOST_LIBRARY NAMES bcm_host libbcm_host HINTS "/opt/vc/lib")
	target_link_libraries(VC4CL ${BCMHOST_LIBRARY} ${SYSROOT_LIBRARY_FLAGS})
endif()
if(ENABLE_COVERAGE)
	target_compile_options(VC4CL PRIVATE -fprofile-arcs -ftest-coverage --coverage)
	target_link_libraries(VC4CL gcov "-fprofile-arcs -ftest-coverage")
endif(ENABLE_COVERAGE)
if(CROSS_COMPILE)
	target_link_libraries(VC4CL rt ${SYSROOT_LIBRARY_FLAGS})
endif()

if(BUILD_ICD)
	# To be exact, this enables the methods for the cl_khr_icd extension to be exported as publicly accessible
	target_compile_definitions(VC4CL PUBLIC -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)

	if(NOT CROSS_COMPILE)
		target_include_directories(VC4CL SYSTEM PRIVATE ${OCL_ICD_INCLUDEDIR})
	endif()
endif()

if(IMAGE_SUPPORT)
	target_compile_definitions(VC4CL PUBLIC -DIMAGE_SUPPORT=1)
endif()

if(REGISTER_POKE_KERNELS)
	target_compile_definitions(VC4CL PUBLIC -DREGISTER_POKE_KERNELS=1)
endif()

# "For shared libraries VERSION and SOVERSION can be used to specify the build version and API version respectively."
set_target_properties(
	VC4CL PROPERTIES
	# This corresponds to the project/library-version
	VERSION "${PROJECT_VERSION}"
	# This corresponds to the API-version (e.g. OpenCL 1.2)
	SOVERSION "1.2"
)

find_program(
	CLANG_FORMAT_PATH
	NAMES clang-format clang-format-3.8 clang-format-3.9 clang-format-4.0 clang-format-5.0 clang-format-6.0 clang-format-7 clang-format-8 clang-format-9
	HINTS "/usr/bin" "/usr/local/bin"
)

if(CLANG_FORMAT_PATH)
	message (STATUS "found clang-format: ${CLANG_FORMAT_PATH}")
	get_target_property(VC4CL_SRCS VC4CL SOURCES)
	add_custom_target(
		clang-format
		COMMAND ${CLANG_FORMAT_PATH}
		-i ${VC4CL_SRCS}
		WORKING_DIRECTORY
		${CMAKE_CURRENT_SOURCE_DIR}
	)
else()
  message (WARNING "clang-format not found: strongly recommend to use it before commit!")
endif()

##
# Git commit in the version string
##
add_custom_target(GetGitCommit
	# If the git command fails, write dummy content (e.g. when not on a git repository)
	COMMAND git log -1 "--format=#define GIT_COMMIT \"%h\"" > ./git_commit.h || echo "#define GIT_COMMIT \"unknown\"" > ./git_commit.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM
)
target_include_directories(VC4CL PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(VC4CL GetGitCommit)

##
# Installation targets
##
# Creates the install target for the library and the tools
install(TARGETS VC4CL EXPORT VC4CL-targets LIBRARY DESTINATION lib)
# Creates the export target (to be used by CMake to find the INSTALLED library)
install(EXPORT VC4CL-targets DESTINATION share/vc4cl)
# Exports the target (to be used by CMake to find the SOURCE library)
export(TARGETS VC4CL FILE vc4cl-exports.cmake)
