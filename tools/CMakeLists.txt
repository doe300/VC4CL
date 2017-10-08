include_directories(..)
include_directories(../include)

add_executable(v3d_info V3DInfo.cpp)
target_link_libraries(v3d_info VC4CL)

add_executable(v3d_profile V3DProfile.cpp)
target_link_libraries(v3d_profile VC4CL)

##
# Installation targets
##
# Creates the install target for the library and the tools
install(TARGETS v3d_info EXPORT v3d_info-targets RUNTIME DESTINATION bin)
install(TARGETS v3d_profile EXPORT v3d_profile-targets RUNTIME DESTINATION bin)