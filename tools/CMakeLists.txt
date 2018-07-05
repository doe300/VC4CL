add_executable(v3d_info "")
target_link_libraries(v3d_info VC4CL ${SYSROOT_LIBRARY_FLAGS})
target_include_directories(v3d_info PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(v3d_info PRIVATE ${OpenCL_INCLUDE_DIRS})

add_executable(v3d_profile "")
target_link_libraries(v3d_profile VC4CL ${SYSROOT_LIBRARY_FLAGS})
target_include_directories(v3d_profile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(v3d_profile PRIVATE ${OpenCL_INCLUDE_DIRS})

add_executable(vc4cl_dump_analyzer "")
target_link_libraries(vc4cl_dump_analyzer VC4CL ${SYSROOT_LIBRARY_FLAGS})
target_include_directories(vc4cl_dump_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(vc4cl_dump_analyzer PRIVATE ${OpenCL_INCLUDE_DIRS})

target_compile_definitions(v3d_info PRIVATE -DVC4CL_LIBRARY_VERSION="${PROJECT_VERSION}")
target_compile_definitions(v3d_profile PRIVATE -DVC4CL_LIBRARY_VERSION="${PROJECT_VERSION}")
target_compile_definitions(vc4cl_dump_analyzer PRIVATE -DVC4CL_LIBRARY_VERSION="${PROJECT_VERSION}")

if(BUILD_ICD)
  target_compile_definitions(v3d_info PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)
  target_compile_definitions(v3d_profile PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)
  target_compile_definitions(vc4cl_dump_analyzer PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)
endif()

if(IMAGE_SUPPORT)
	target_compile_definitions(v3d_info PRIVATE -DIMAGE_SUPPORT=1)
endif()

if(REGISTER_POKE_KERNELS)
	target_compile_definitions(v3d_info PRIVATE -DREGISTER_POKE_KERNELS=1)
endif()

include(sources.list)
##
# Installation targets
##
# Creates the install target for the library and the tools
install(TARGETS v3d_info EXPORT v3d_info-targets RUNTIME DESTINATION bin)
install(TARGETS v3d_profile EXPORT v3d_profile-targets RUNTIME DESTINATION bin)
install(TARGETS vc4cl_dump_analyzer EXPORT vc4cl_dump_analyzer-targets RUNTIME DESTINATION bin)
