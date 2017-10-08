#Include headers in the project settings (as search-path for header-files)
include_directories ("${PROJECT_SOURCE_DIR}/src")

#Include headers in the project structure
file( GLOB HDRS *.h)
#Include the source files
file( GLOB SRCS *.cpp */*.cpp)

#Build QPUAsm
add_library( VC4CL SHARED ${SRCS} ${HDRS} )

if(INCLUDE_COMPILER AND EXISTS "${PROJECT_SOURCE_DIR}/lib/vc4c/include/c_interface.h")
	find_library(VC4CC_LIBRARY NAMES VC4CC libVC4CC HINTS "${PROJECT_SOURCE_DIR}/lib/vc4c/build")
	target_link_libraries(VC4CL ${VC4CC_LIBRARY})
endif()

if(CROSS_COMPILE OR EXISTS "/opt/vc/include/bcm_host.h")
	find_library(BCMHOST_LIBRARY NAMES bcm_host libbcm_host HINTS "/opt/vc/lib")
	target_link_libraries(VC4CL ${BCMHOST_LIBRARY})
endif()

if(CROSS_COMPILE)
	target_link_libraries(VC4CL rt)
endif()

# "For shared libraries VERSION and SOVERSION can be used to specify the build version and API version respectively."
set_target_properties(
	VC4CL PROPERTIES
	# This corresponds to the project/library-version
	VERSION "${PROJECT_VERSION}"
	# This corresponds to the API-version (e.g. OpenCL 1.2)
	SOVERSION "1.2"
)

##
# Installation targets
##
# Creates the install target for the library and the tools
install(TARGETS VC4CL EXPORT VC4CL-targets LIBRARY DESTINATION lib)
# Creates the export target (to be used by CMake to find the INSTALLED library)
install(EXPORT VC4CL-targets DESTINATION share/vc4cl)
# Exports the target (to be used by CMake to find the SOURCE library)
export(TARGETS VC4CL FILE vc4cl-exports.cmake)
