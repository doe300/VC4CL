# Adds custom uninstall command
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall_vc4cl.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall_vc4cl.cmake"
  IMMEDIATE @ONLY)
add_custom_target(uninstall_vc4cl 
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall_vc4cl.cmake)

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/VC4CL.icd
  COMMAND echo "${CMAKE_INSTALL_PREFIX}/lib/libVC4CL.so" > ${PROJECT_BINARY_DIR}/VC4CL.icd
  )

if(BUILD_ICD)
	add_custom_target(generate_icd DEPENDS VC4CL.icd)
	add_dependencies(VC4CL generate_icd)
	install(FILES ${PROJECT_BINARY_DIR}/VC4CL.icd DESTINATION "/etc/OpenCL/vendors")
endif()