add_executable(v3d_info "")
target_link_libraries(v3d_info VC4CL ${SYSROOT_LIBRARY_FLAGS})
target_include_directories(v3d_info PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_options(v3d_info PRIVATE ${VC4CL_ENABLED_WARNINGS})

if(CURSES_FOUND)
  add_executable(v3d_profile "")
  target_link_libraries(v3d_profile VC4CL ${SYSROOT_LIBRARY_FLAGS})
  target_link_libraries(v3d_profile VC4CL ${CURSES_LIBRARIES})
  target_include_directories(v3d_profile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
  target_include_directories(v3d_profile PRIVATE ${CURSES_INCLUDE_DIRS})
  target_compile_options(v3d_profile PRIVATE ${VC4CL_ENABLED_WARNINGS})
endif()

if(INCLUDE_COMPILER AND EXISTS "${VC4C_HEADER_PATH}")
	set(VC4CL_BUILD_DUMP_ANALYZER ON)
	add_executable(vc4cl_dump_analyzer "")
	target_link_libraries(vc4cl_dump_analyzer VC4CL ${SYSROOT_LIBRARY_FLAGS})
	target_include_directories(vc4cl_dump_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
	target_compile_options(vc4cl_dump_analyzer PRIVATE ${VC4CL_ENABLED_WARNINGS})
endif()

if(BUILD_ICD)
  target_compile_definitions(v3d_info PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)
  if(CURSES_FOUND)
    target_compile_definitions(v3d_profile PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)
  endif()
  if(VC4CL_BUILD_DUMP_ANALYZER)
    target_compile_definitions(vc4cl_dump_analyzer PRIVATE -Dcl_khr_icd=1 -Duse_cl_khr_icd=1)
  endif()
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
if(CURSES_FOUND)
  install(TARGETS v3d_profile EXPORT v3d_profile-targets RUNTIME DESTINATION bin)
endif()
if(VC4CL_BUILD_DUMP_ANALYZER)
  install(TARGETS vc4cl_dump_analyzer EXPORT vc4cl_dump_analyzer-targets RUNTIME DESTINATION bin)
endif()
