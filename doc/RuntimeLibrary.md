
## Status of the support of the OpenCL runtime library

Sources: [OpenCL 1.2 Reference Pages](https://www.khronos.org/registry/cl/sdk/1.2/docs/man/xhtml/), section OpenCL Platform,
[OpenCL 1.2 Specification](https://www.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf), 
[OpenCL 1.2 Extension Specification](https://www.khronos.org/registry/OpenCL/specs/opencl-1.2-extensions.pdf)

### General
Only *EMBEDDED_PROFILE* is supported, since the implementation does not support 64-bit integers. 
More so, the 64-bit and 16-bit floating point extensions are unsupported, as well as anything which has to do with images or OpenGL inter-operability. 
Since access to the memory (shared with the CPU), is always synchronized among all QPUs, all *cl_khr_xxx_atomics* extensions are supported.

### Query Platform Info
* **clGetPlatformIDs** - returns a single platform for the VideoCore IV implementation
* **clGetPlatformInfo** - supports all parameters specified in OpenCL 1.2

### Query Devices
* **clGetDeviceIDs** - returns a single device of the type *CL_DEVICE_TYPE_GPU* (which is also *CL_DEVICE_TYPE_DEFAULT*) representing the VideoCore IV GPU
* **clGetDeviceInfo** - supports all parameters specified in OpenCL 1.2. Supports 1 compute unit with 12 work-items (12 QPUs) has a preferred vector-width or all supported types of 16 elements (since a QPU is a virtual 16-way SIMD). Long, Double and Half types are unsupported. The device is a 32-bit lower endian processor and has a default clock rate of 250MHz. The graphic memory is shared with the host memory and its maximum size can be configured via the */boot/config.txt* or tools such as *raspi-config* (the setting is called memory-split). Images are not supported, neither is printf and device-partitioning/sub-device creation. Compiler is supported, if the library is compiled with the VC4C compiler.

### Partition a Device
* **clCreateSubDevices** - not supported
* **clRetainDevice** - not supported
* **clReleaseDevice** - not supported

### Contexts
* **clCreateContext** - supported
* **clCreateContextFromType** - supported
* **clRetainContext** - supported
* **clReleaseContext** - supported, automatically cleans up context and all its children object
* **clGetContextInfo** - supports all parameters specified in OpenCL 1.2.

### Command Queues
* **clCreateCommandQueue** - supported, commands are always executed in-order, disregarding the value of *CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE*. Profiling is supported if and only if *CL_QUEUE_PROFILING_ENABLE* is specified.
* **clRetainCommandQueue** - supported
* **clReleaseCommandQueue** - supported, automatically cleans up command queue and all its children
* **clGetCommandQueueInfo** - supports all parameters specified in OpenCL 1.2.

### Buffer Objects
* **clCreateBuffer** - supported, the buffer is immediately allocated on the device-memory
* **clCreateSubBuffer** - supported
* **clEnqueueReadBuffer** - supported
* **clEnqueueWriteBuffer** - supported
* **clEnqueueReadBufferRect** - supported
* **clEnqueueWriteBufferRect** - supported
* **clEnqueueFillBuffer** - supported
* **clEnqueueCopyBuffer** - supported
* **clEnqueueCopyBufferRect** - supported
* **clEnqueueMapBuffer** - supported, device-memory is always mapped to host-memory
* **clCreateImage** - images are not supported
* **clGetSupportedImageFormats** - images are not supported
* **clEnqueueReadImage** - images are not supported
* **clEnqueueWriteImage** - images are not supported
* **clEnqueueFillImage** - images are not supported
* **clEnqueueCopyImage** - images are not supported
* **clEnqueueCopyImageToBuffer** - images are not supported
* **clEnqueueCopyBufferToImage** - images are not supported
* **clEnqueueMapImage** - images are not supported
* **clEnqueueUnmapMemObject** - supported, device-memory is always mapped to host-memory
* **clEnqueueMigrateMemObjects** - **TBD**
* **clGetMemObjectInfo** - supports all parameters specified in OpenCL 1.2
* **clGetImageInfo** - images are not supported
* **clRetainMemObject** - supported
* **clReleaseMemObject** - supported, automatically cleans up device memory
* **clSetMemObjectDestructorCallback** - supported

### Sampler Objects
* **clCreateSampler** - always returns *CL_INVALID_OPERATION*, since images and samplers are not supported
* **clRetainSampler** - supported, but no-op
* **clReleaseSampler** - supported, but no-op
* **clGetSamplerInfo** - supported, always returns *CL_INVALID_SAMPLER*, since images and samplers are not supported

### Program Objects
* **clCreateProgramWithSource** - supported, supports OpenCL source as well as LLVM IR and SPIR-V as source code
* **clCreateProgramWithBinary** - supported, supports platform-dependent binary format (e.g. as output of VC4C)
* **clCreateProgramWithBuiltInKernels** - supported, but there are no supported built-in kernels
* **clRetainProgram** - supported
* **clReleaseProgram** - supported, releases program and all its resources automatically
* **clBuildProgram** - only supported, when compiled with the VC4C compiler
* **clCompileProgram** - only supported, when compiled with the VC4C compiler
* **clLinkProgram** - supported. **NOTE**: Linking of OpenCL programs is not supported, internally this method extracts the kernels from the binary
* **clUnloadPlatformCompiler** - supported, but no-op
* **clGetProgramInfo** - supports all parameters specified in OpenCL 1.2
* **clGetProgramBuildInfo** - supported

### Kernel Objects 
* **clCreateKernel** - supported
* **clCreateKernelsInProgram** - supported
* **clRetainKernel** - supported
* **clReleaseKernel** - supported, the kernel and all its resources are automatically freed
* **clSetKernelArg** - supported
* **clGetKernelInfo** - supports all parameters specified in OpenCL 1.2. There is no way to retrieve the content of *CL_KERNEL_ATTRIBUTES*.
* **clGetKernelWorkGroupInfo** - supported
* **clGetKernelArgInfo** - supports all parameters specified in OpenCL 1.2.

### Executing Kernels
* **clEnqueueNDRangeKernel** - supported
* **clEnqueueTask** - supported
* **clEnqueueNativeKernel** - supported, but no native kernels available

### Event Objects 
* **clCreateUserEvent** - supported
* **clSetUserEventStatus** - supported
* **clWaitForEvents** - supported
* **clGetEventInfo** - supports all parameters specified in OpenCL 1.2.
* **clSetEventCallback** - supported
* **clRetainEvent** - supported
* **clReleaseEvent** - supported, automatically frees the event

### Markers, Barriers, and Waiting 
* **clEnqueueMarkerWithWaitList** - supported
* **clEnqueueBarrierWithWaitList** - supported

### Profiling Operations on Memory Objects and Kernels 
* **clGetEventProfilingInfo** - supports all parameters specified in OpenCL 1.2.

### Flush and Finish 
* **clFlush** - supported, but is no-op since all events are automatically flushed
* **clFinish** - supported

## Extensions
### Khronos ICD loader (cl_khr_icd)
See [ICD loader specification](https://www.khronos.org/registry/OpenCL/extensions/khr/cl_khr_icd.txt) for details.

* **clIcdGetPlatformIDsKHR** - supported, returns the 1 global platform
* **clGetExtensionFunctionAddress** - supported, but there are no extension functions

### Intermediate Language Programs (cl_khr_il_program)
See [Intermediate Language Programs](https://www.khronos.org/registry/OpenCL/specs/opencl-1.2-extensions.pdf#page=153) for details.

* **clCreateProgramWithILKHR** - supported, but optional, since **CreateProgramWithSource** also accepts SPIR-V source

### Querying device temperature (cl_altera_device_temperature)
See [extension specification](https://www.khronos.org/registry/OpenCL/extensions/altera/cl_altera_device_temperature.txt) for details.