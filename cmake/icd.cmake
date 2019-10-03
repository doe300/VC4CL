if(BUILD_ICD)
	message(STATUS "Building with ICD support")

	if(NOT CROSS_COMPILE)
		# accoding to pocl, pkg-config doesn't work with cross-compiling
		pkg_search_module(OCL_ICD REQUIRED ocl-icd>=1.3)
		message(STATUS "Found Khronos ICD Loader in version ${OCL_ICD_VERSION} in ${OCL_ICD_LIBDIR}")
	endif()

	# Check clCreateProgramWithIL exist or not
	set(CMAKE_REQUIRED_LIBRARIES OpenCL)
	check_symbol_exists(clCreateProgramWithIL ${SYSROOT_CROSS}/usr/include/CL/cl.h AVAIL_clCreateProgramWithIL)
	if(NOT AVAIL_clCreateProgramWithIL)
		message(WARNING "clCreateProgramWithIL not found, strongly recommend to upgrade package opencl-headers!")
	endif()
	set(CMAKE_REQUIRED_LIBRARIES)
endif()