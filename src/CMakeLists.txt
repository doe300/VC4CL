#Include headers in the project settings (as search-path for header-files)
include_directories ("${PROJECT_SOURCE_DIR}/src")

#Include headers in the project structure
file( GLOB HDRS *.h)
#Include the source files
file( GLOB SRCS *.cpp */*.cpp)

#Build QPUAsm
add_library( VC4CL SHARED ${SRCS} ${HDRS} )

if(INCLUDE_COMPILER AND EXISTS "${VC4C_HEADER_PATH}")
	target_link_libraries(VC4CL ${VC4CC_LIBRARY} ${SYSROOT_LIBRARY_FLAGS})
endif()

if(CROSS_COMPILE OR EXISTS "/opt/vc/include/bcm_host.h")
	find_library(BCMHOST_LIBRARY NAMES bcm_host libbcm_host HINTS "/opt/vc/lib")
	target_link_libraries(VC4CL ${BCMHOST_LIBRARY} ${SYSROOT_LIBRARY_FLAGS})
endif()

if(CROSS_COMPILE)
	target_link_libraries(VC4CL rt ${SYSROOT_LIBRARY_FLAGS})
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
