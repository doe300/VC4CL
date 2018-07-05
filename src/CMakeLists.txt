add_library( VC4CL SHARED "")
target_compile_definitions(VC4CL PRIVATE -DVC4CL_LIBRARY_VERSION="${PROJECT_VERSION}")
target_include_directories(VC4CL PRIVATE ${OpenCL_INCLUDE_DIRS})

# append usage of C++ to compiler flags, also optimize for speed and enable all warnings
set(VC4CL_ENABLED_WARNINGS -Wall -Wextra -Wold-style-cast -Wno-unused-parameter -Wno-missing-field-initializers -Wno-write-strings)
# Enable additional warnings, if available
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	SET(VC4CL_ENABLED_WARNINGS ${VC4CL_ENABLED_WARNINGS} -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-shadow -Wno-padded -Wno-shadow-field-in-constructor -Wno-global-constructors -Wno-exit-time-destructors -Wno-missing-prototypes -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-documentation -Wno-unused-command-line-argument -Wno-unused-member-function -Wno-gnu-zero-variadic-macro-arguments -Wno-covered-switch-default -Wno-switch-enum -Wswitch)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	SET(VC4CL_ENABLED_WARNINGS ${VC4CL_ENABLED_WARNINGS} -Wdouble-promotion -fdelete-null-pointer-checks -Wnull-dereference -Wuninitialized -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=format -Wsuggest-override -Wconversion -Wzero-as-null-pointer-constant)
endif()
target_compile_options(VC4CL PRIVATE ${VC4CL_ENABLED_WARNINGS})

if(BUILD_DEBUG)
	target_compile_definitions(VC4CL PRIVATE DEBUG_MODE=1)
endif(BUILD_DEBUG)

# Add all sources
include(sources.list)

if(INCLUDE_COMPILER AND EXISTS "${VC4C_HEADER_PATH}")
	target_compile_definitions(VC4CL PRIVATE -DCOMPILER_HEADER="${VC4C_HEADER_PATH}" -DVC4C_TOOLS_HEADER="${VC4C_TOOLS_HEADER_PATH}" -DHAS_COMPILER=1)
	target_link_libraries(VC4CL ${VC4CC_LIBRARY} ${SYSROOT_LIBRARY_FLAGS})
endif()

if(CROSS_COMPILE OR EXISTS "/opt/vc/include/bcm_host.h")
	find_library(BCMHOST_LIBRARY NAMES bcm_host libbcm_host HINTS "/opt/vc/lib")
	target_link_libraries(VC4CL ${BCMHOST_LIBRARY} ${SYSROOT_LIBRARY_FLAGS})
endif()

if(CROSS_COMPILE)
	target_link_libraries(VC4CL rt ${SYSROOT_LIBRARY_FLAGS})
endif()

if(BUILD_ICD)
	# To be exact, this enables the methods for the cl_khr_icd extension to be exported as publicly accessible
	target_compile_definitions(VC4CL PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)

	if(NOT CROSS_COMPILE)
		target_include_directories(VC4CL PRIVATE ${OCL_ICD_INCLUDEDIR})
	endif()
endif()

if(IMAGE_SUPPORT)
	target_compile_definitions(VC4CL PRIVATE -DIMAGE_SUPPORT=1)
endif()

if(REGISTER_POKE_KERNELS)
	target_compile_definitions(VC4CL PRIVATE -DREGISTER_POKE_KERNELS=1)
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
	NAMES clang-format clang-format-3.8 clang-format-3.9 clang-format-4.0
	HINTS "/usr/bin" "/usr/local/bin"
)

if(CLANG_FORMAT_PATH)
	message (STATUS "found clang-format: ${CLANG_FORMAT_PATH}")
	add_custom_target(
		clang-format
		COMMAND ${CLANG_FORMAT_PATH}
		-i ${SRCS} ${HDRS}
	)
else()
  message (WARNING "clang-format not found: strongly recommend to use it before commit!")
endif()

##
# Installation targets
##
# Creates the install target for the library and the tools
install(TARGETS VC4CL EXPORT VC4CL-targets LIBRARY DESTINATION lib)
# Creates the export target (to be used by CMake to find the INSTALLED library)
install(EXPORT VC4CL-targets DESTINATION share/vc4cl)
# Exports the target (to be used by CMake to find the SOURCE library)
export(TARGETS VC4CL FILE vc4cl-exports.cmake)
