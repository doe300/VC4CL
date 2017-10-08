/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Kernel.h"
#include "Buffer.h"
#include "Device.h"
#include "SVM.h"
#include "V3D.h"

#include "extensions.h"

using namespace vc4cl;

void KernelArgument::addScalar(const float f)
{
	ScalarValue v;
	v.f = f;
	scalarValues.push_back(v);
}

void KernelArgument::addScalar(const uint32_t u)
{
	ScalarValue v;
	v.u = u;
	scalarValues.push_back(v);
}

void KernelArgument::addScalar(const int32_t s)
{
	ScalarValue v;
	v.s = s;
	scalarValues.push_back(v);
}

void KernelArgument::addScalar(void* ptr)
{
	ScalarValue v;
	v.ptr = ptr;
	scalarValues.push_back(v);
}

std::string KernelArgument::to_string() const
{
	std::string res;
	for(const ScalarValue& v : scalarValues)
	{
		res += std::to_string(v.u) + ", ";
	}
	return res.substr(0, res.length() - 2);
}

Kernel::Kernel(Program* program, const KernelInfo& info) : program(program), info(info), argsSetMask(0)
{
	args.resize(info.params.size());
	if(program->retain() != CL_SUCCESS)
		throw std::runtime_error("Failed to retain program in kernel!");
}

Kernel::~Kernel()
{
	ignoreReturnValue(program->release(), __FILE__, __LINE__, "There is no way of handling an error here");
}

cl_int Kernel::setArg(cl_uint arg_index, size_t arg_size, const void* arg_value, const bool isSVMPointer)
{
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Set kernel arg " << arg_index << " for kernel '" << info.name << "' to " << arg_value << " (" << (arg_value == nullptr ? 0x0 : *(int*)arg_value) << ") with size " << arg_size << std::endl;
	std::cout << "[VC4CL] Kernel arg " << arg_index << " for kernel '" << info.name << "' is " << info.params[arg_index].type << " '" << info.params[arg_index].name << "' with size " << static_cast<size_t>(info.params[arg_index].size) << std::endl;
#endif

	if(arg_index >= info.params.size())
	{
		return returnError(CL_INVALID_ARG_INDEX, __FILE__, __LINE__, buildString("Invalid arg index: %d of %d", arg_index, info.params.size()));
	}

	//clear previous set parameter value
	args[arg_index] = KernelArgument();

	if(info.params[arg_index].pointer == CL_FALSE)
	{
		//literal (scalar or vector) argument
		if(arg_size != info.params[arg_index].size)
		{
			return returnError(CL_INVALID_ARG_SIZE, __FILE__, __LINE__, buildString("Invalid arg size: %u, must be %d", arg_size, info.params[arg_index].size));
		}
		const cl_char elementSize = arg_size / info.params[arg_index].elements;
		for(cl_uchar i = 0; i < info.params[arg_index].elements; ++i)
		{
			//arguments are all 32-bit, since UNIFORMS are always 32-bit
			cl_uint tmp = 0;
			if(elementSize == 1 /* [u]char-types */)
			{
				//expand 8-bit to 32-bit
				tmp = 0xFF & ((cl_uchar*)arg_value)[i];
				args[arg_index].addScalar(tmp);
			}
			else if(elementSize == 2 /* [u]short-types */)
			{
				//expand 16-bit to 32-bit
				tmp = 0xFFFF & ((cl_ushort*)arg_value)[i];
				args[arg_index].addScalar(tmp);
			}
			else if(elementSize > 4)
			{
				//not supported
				return returnError(CL_INVALID_ARG_SIZE, __FILE__, __LINE__, buildString("Invalid arg size: %u", arg_size));
			}
			else
			{
				args[arg_index].addScalar(((cl_uint*)arg_value)[i]);
			}
		}
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Setting kernel-argument " << arg_index << " to " << args[arg_index].to_string() << std::endl;
#endif
	}
	else
	{
		//argument is pointer to object, e.g. buffer, image (which too is a buffer), sampler
		//"If the argument is a memory object (buffer, image or image array), the arg_value entry will be a pointer to the appropriate buffer [...]"
		//"If the argument is a buffer object, the arg_value pointer can be NULL or point to a NULL value
		// in which case a NULL value will be used as the value for the argument declared as a pointer"
		//"If the argument is declared to be a pointer of a built-in scalar or vector type [...] the memory object specified as argument value must be a buffer object (or NULL)"
		// -> no pointers to non-buffer objects are allowed! -> good, no extra checking required
		void* pointer_arg = NULL;
		if(arg_value != NULL && *((void**)arg_value) != NULL)
		{
			if(isSVMPointer)
			{
				//SVM pointers are passed as direct GPU pointers (and are checked before), so the usual check for valid Buffer does not apply here
				pointer_arg = const_cast<void*>(arg_value);
			}
			else
			{
				//"If the argument is a memory object, the size is the size of the buffer or image object type."
				if(arg_size != sizeof(cl_mem))
				{
					return returnError(CL_INVALID_ARG_SIZE, __FILE__, __LINE__, buildString("Invalid arg size for buffers: %d, must be %u", arg_size, sizeof(cl_mem)));
				}

				cl_mem buffer = *(cl_mem*)arg_value;
				CHECK_BUFFER(toType<Buffer>(buffer))
				if(toType<Buffer>(buffer)->context() != program->context())
					return returnError(CL_INVALID_ARG_VALUE, __FILE__, __LINE__, buildString("Contexts of buffer and program do not match: %p != %p", toType<Buffer>(buffer)->context(), program->context()));
				if(info.params[arg_index].output && !toType<Buffer>(buffer)->writeable)
					//TODO some OpenCL-CTS test-cases fail here! (e.g. buffer_map_read_short)
					return returnError(CL_INVALID_ARG_VALUE, __FILE__, __LINE__, "Setting a non-writeable buffer as output parameter!");
				if(info.params[arg_index].input && !toType<Buffer>(buffer)->readable)
					return returnError(CL_INVALID_ARG_VALUE, __FILE__, __LINE__, "Setting a non-readable buffer as input parameter!");
				pointer_arg = toType<Buffer>(buffer)->deviceBuffer->qpuPointer;
			}
		}
		//"If the argument is declared with the __local qualifier, the arg_value entry must be NULL."
		//"For arguments declared with the __local qualifier, the size specified will be the size in bytes of the buffer that must be allocated for the __local argument"
		//TODO how to check if argument is local? And set size of buffer to be allocated + somewhere need to allocate the extra buffer
		args[arg_index].addScalar(pointer_arg);
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Setting kernel-argument " << arg_index << " to " << pointer_arg << std::endl;
#endif
	}

	argsSetMask |= 1 << arg_index;

	return CL_SUCCESS;
}

cl_int Kernel::getInfo(cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	switch(param_name)
	{
		case CL_KERNEL_FUNCTION_NAME:
			return returnString(info.name, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_NUM_ARGS:
			return returnValue<cl_uint>(info.params.size(), param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_REFERENCE_COUNT:
			return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_CONTEXT:
			return returnValue<cl_context>(program->context()->toBase(), param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_PROGRAM:
			return returnValue<cl_program>(program->toBase(), param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_ATTRIBUTES:
			return CL_INVALID_VALUE;
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_kernel_info value %d", param_name));
}

cl_int Kernel::getWorkGroupInfo(cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	switch(param_name)
	{
		case CL_KERNEL_GLOBAL_WORK_SIZE:
			//"CL_INVALID_VALUE if param_name is CL_KERNEL_GLOBAL_WORK_SIZE and device is not a custom device or kernel is not a built-in kernel."
			return CL_INVALID_VALUE;
		case CL_KERNEL_WORK_GROUP_SIZE:
			//"[...] query the maximum work-group size that can be used to execute a kernel on a specific device [...]"
			return returnValue<size_t>(V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT), param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_COMPILE_WORK_GROUP_SIZE:
			return returnValue(info.compileGroupSizes, sizeof(size_t), 3, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_LOCAL_MEM_SIZE:
			return returnValue<cl_ulong>(0, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
			return returnValue<size_t>(1, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_PRIVATE_MEM_SIZE:
			return returnValue<cl_ulong>(0, param_value_size, param_value, param_value_size_ret);
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_kernel_work_group_info value %d", param_value));
}

cl_int Kernel::getArgInfo(cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	if(arg_index >= info.params.size())
		return returnError(CL_INVALID_ARG_INDEX, __FILE__, __LINE__, buildString("Invalid argument index %u (of %u)", arg_index, info.params.size()));

	const ParamInfo& paramInfo = info.params[arg_index];

	switch(param_name)
	{
		case CL_KERNEL_ARG_ADDRESS_QUALIFIER:
			return (paramInfo.addressSpace == AddressSpace::CONSTANT ? CL_KERNEL_ARG_ADDRESS_CONSTANT : (paramInfo.addressSpace == AddressSpace::GLOBAL ? CL_KERNEL_ARG_ADDRESS_GLOBAL : (paramInfo.addressSpace == AddressSpace::LOCAL ? CL_KERNEL_ARG_ADDRESS_LOCAL : CL_KERNEL_ARG_ADDRESS_PRIVATE)));
		case CL_KERNEL_ARG_ACCESS_QUALIFIER:
			//"If argument is not an image type, CL_KERNEL_ARG_ACCESS_NONE is returned"
			return returnValue<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_NONE, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_ARG_TYPE_NAME:
			return returnString(paramInfo.type, param_value_size, param_value, param_value_size_ret);
		case CL_KERNEL_ARG_TYPE_QUALIFIER:
			return (paramInfo.constant ? CL_KERNEL_ARG_TYPE_CONST : 0) | (paramInfo.restricted ? CL_KERNEL_ARG_TYPE_RESTRICT : 0) |
					(paramInfo.isVolatile ? CL_KERNEL_ARG_TYPE_VOLATILE : 0);
		case CL_KERNEL_ARG_NAME:
			return returnString(paramInfo.name, param_value_size, param_value, param_value_size_ret);
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_kernel_arg_info value %d", param_name));
}

/*
 * Tries to split the global sizes into the sizes specified at compile-time
 */
static cl_bool split_compile_work_size(size_t compile_group_sizes[VC4CL_NUM_DIMENSIONS], const size_t* global_sizes, size_t* local_sizes)
{
	if(compile_group_sizes[0] == 0 && compile_group_sizes[1] == 0 && compile_group_sizes[2] == 0)
		//no compile-time sizes set
		return CL_FALSE;
	const cl_uint max_group_size = V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT);

	if((global_sizes[0] % compile_group_sizes[0]) != 0 || (global_sizes[1] % compile_group_sizes[1]) != 0 || (global_sizes[2] % compile_group_sizes[2]) != 0)
		//doesn't fit into compile-time sizes
		return CL_FALSE;

	if(compile_group_sizes[0] * compile_group_sizes[1] * compile_group_sizes[2] > max_group_size)
		//would fit into compile-time sizes, but are too many work-items in group
		return CL_FALSE;

	local_sizes[0] = compile_group_sizes[0];
	local_sizes[1] = compile_group_sizes[1];
	local_sizes[2] = compile_group_sizes[2];
	return CL_TRUE;
}

/*
 * Needs to divide the global_sites into local_sizes, so that:
 * - the size of a work-group (product of all local_sizes) does not exceed the number of QPUs
 * - the size of a work-group is as large as possible, which is equivalent to
 * - the number of work-groups is as small as possible
 */
static cl_int split_global_work_size(const size_t* global_sizes, size_t* local_sizes, cl_uint num_dimensions)
{
	const size_t total_sizes = global_sizes[0] * global_sizes[1] * global_sizes[2];
	const cl_uint max_group_size = V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT);
	if(total_sizes <= max_group_size)
	{
		//can be executed in a single work-group
		local_sizes[0] = global_sizes[0];
		local_sizes[1] = global_sizes[1];
		local_sizes[2] = global_sizes[2];
		return CL_SUCCESS;
	}
	/*
	 * Method 1: split all dimensions through the number of work-items. This results in following relation:
	 * global[0] = x * local[0]
	 * global[1] = x * local[1]
	 * global[2] = x * local[2]
	 * -> produces x� work-groups
	 * - only works, if global[0,1,2] are all divisible by the same number
	 */
	/*
	 * Method 2: split only the first dimension, resulting in following relation:
	 * global[0] = x * local[0]
	 * global[1] = global[1] * local[1]	(local[1] is 1)
	 * global[2] = global[2] * local[2]	(local[2] is 1)
	 * -> produces x * global[1] * global[2] work-groups
	 * - works always, wastes QPUs if global[0] smaller than the number of QPUs
	 */

	//for now choose method 2
	for(cl_uint work_group_size = max_group_size; work_group_size > 0; --work_group_size)
	{
		//starting by all QPUs, try to determine a number of QPUs, which can be used,
		// so the total number of items can be evenly distributed among them
		if(global_sizes[0] % work_group_size == 0)
		{
			local_sizes[0] = work_group_size;
			local_sizes[1] = 1;
			local_sizes[2] = 1;
			//last, check whether the number of work-items in a work-group fits into the limit
			if(local_sizes[0] * local_sizes[1] * local_sizes[2] <= max_group_size)
			{
				//we found an acceptable distribution
#ifdef DEBUG_MODE
				std::cout << "[VC4CL] Splitting " << global_sizes[0] << " * " << global_sizes[1] << " * " << global_sizes[2] << " work-items into " << local_sizes[0] << " * " << local_sizes[1] << " * " << local_sizes[2] << ", using " << work_group_size << " of " << max_group_size << " QPUs" << std::endl;
#endif
				return CL_SUCCESS;
			}
		}
	}

	//we didn't find any good distribution
	return returnError(CL_INVALID_WORK_GROUP_SIZE, __FILE__, __LINE__, "Failed to find a matching local work size!");
}

cl_int Kernel::enqueueNDRange(CommandQueue* commandQueue, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list,
		const cl_event* event_wait_list, cl_event* event)
{
	if(commandQueue->context() != program->context())
	{
		return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of command queue and program do not match!");
	}

	if(program->kernelInfo.empty())
	{
		return returnError(CL_INVALID_PROGRAM_EXECUTABLE, __FILE__, __LINE__, "Kernel was not yet compiled!");
	}

	if(argsSetMask != static_cast<cl_ulong>((1 << info.params.size()) - 1))
	{
		return returnError(CL_INVALID_KERNEL_ARGS, __FILE__, __LINE__, "Not all kernel-arguments are set!");
	}

	if(work_dim > VC4CL_NUM_DIMENSIONS || work_dim < 1)
	{
		return returnError(CL_INVALID_WORK_DIMENSION, __FILE__, __LINE__, buildString("Illegal number of work-group dimensions: %u (of 1 to %u)", work_dim, VC4CL_NUM_DIMENSIONS));
	}

	if(global_work_size == NULL)
	{
		return returnError(CL_INVALID_GLOBAL_WORK_SIZE, __FILE__, __LINE__, "Global-work-size is not set!");
	}

	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

	size_t work_offsets[VC4CL_NUM_DIMENSIONS];
	size_t work_sizes[VC4CL_NUM_DIMENSIONS];
	size_t local_sizes[VC4CL_NUM_DIMENSIONS];
	if(global_work_offset == NULL)
		memset(work_offsets, 0, VC4CL_NUM_DIMENSIONS * sizeof(size_t));
	else
		memcpy(work_offsets, global_work_offset, work_dim * sizeof(size_t));
	memcpy(work_sizes, global_work_size, work_dim * sizeof(size_t));
	//fill to 3 dimensions
	for(size_t i = work_dim; i < VC4CL_NUM_DIMENSIONS; ++i)
	{
		work_offsets[i] = 0;
		work_sizes[i] = 1;
		local_sizes[i] = 1;
	}
	if(local_work_size == NULL)
	{
		//"local_work_size can also be a NULL value in which case the OpenCL implementation
		// will determine how to be break the global work-items into appropriate work-group instances."
		cl_int state = CL_SUCCESS;
		if(split_compile_work_size(info.compileGroupSizes, work_sizes, local_sizes) == CL_FALSE)
		{
			state = split_global_work_size(work_sizes, local_sizes, work_dim);
		}

		if(state != CL_SUCCESS)
		{
			return returnError(state, __FILE__, __LINE__, buildString("Error splitting the global-work-size into local-work-sizes: %u, %u, %u", work_sizes[0], work_sizes[1], work_sizes[2]));
		}
	}
	else
		memcpy(local_sizes, local_work_size, work_dim * sizeof(size_t));
	if(work_sizes[0] > VC4CL_MAX_WORK_ITEM_DIMENSIONS[0] ||
			work_sizes[1] > VC4CL_MAX_WORK_ITEM_DIMENSIONS[1] ||
			work_sizes[2] > VC4CL_MAX_WORK_ITEM_DIMENSIONS[2])
	{
		return returnError(CL_INVALID_GLOBAL_WORK_SIZE, __FILE__, __LINE__, buildString("Global-work-size exceeds maxima: %u (%u), %u (%u), %u (%u)", work_sizes[0], VC4CL_MAX_WORK_ITEM_DIMENSIONS[0], work_sizes[1], VC4CL_MAX_WORK_ITEM_DIMENSIONS[1], work_sizes[2], VC4CL_MAX_WORK_ITEM_DIMENSIONS[2]));
	}
	if(work_sizes[0] + work_offsets[0] > VC4CL_MAX_WORK_ITEM_DIMENSIONS[0] ||
			work_sizes[1] + work_offsets[1] > VC4CL_MAX_WORK_ITEM_DIMENSIONS[1] ||
			work_sizes[2] + work_offsets[2] > VC4CL_MAX_WORK_ITEM_DIMENSIONS[2])
	{
		return returnError(CL_INVALID_GLOBAL_OFFSET, __FILE__, __LINE__, buildString("Global-work-size and offset exceeds maxima: %u (%u), %u (%u), %u (%u)", work_sizes[0] + work_offsets[0], VC4CL_MAX_WORK_ITEM_DIMENSIONS[0], work_sizes[1] + work_offsets[1], VC4CL_MAX_WORK_ITEM_DIMENSIONS[1], work_sizes[2] + work_offsets[2], VC4CL_MAX_WORK_ITEM_DIMENSIONS[2]));
	}
	if(local_sizes[0] * local_sizes[1] * local_sizes[2] > V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT))
		return returnError(CL_INVALID_WORK_GROUP_SIZE, __FILE__, __LINE__, buildString("Local work-sizes exceed maximum: %u * %u * %u > %u", local_sizes[0], local_sizes[1], local_sizes[2], V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT)));

	//check divisibility of local_sizes[i] by work_sizes[i]
	for(cl_uint i = 0; i < VC4CL_NUM_DIMENSIONS; ++i)
	{
		if(work_sizes[i] % local_sizes[i] != 0)
		{
			return returnError(CL_INVALID_WORK_GROUP_SIZE, __FILE__, __LINE__, buildString("Global-work-size is not divisible by local-work-size: %u and %u", work_sizes[i], local_sizes[i]));
		}
	}

	Event* kernelEvent = newObject<Event>(program->context(), CL_QUEUED, CommandType::KERNEL_NDRANGE);
	CHECK_ALLOCATION(kernelEvent)

	KernelSource* source = newObject<KernelSource>(this);
	CHECK_ALLOCATION(source)
	source->numDimensions = work_dim;
	memcpy(source->globalOffsets, work_offsets, VC4CL_NUM_DIMENSIONS * sizeof(size_t));
	memcpy(source->globalSizes, work_sizes, VC4CL_NUM_DIMENSIONS * sizeof(size_t));
	memcpy(source->localSizes, local_sizes, VC4CL_NUM_DIMENSIONS * sizeof(size_t));

	kernelEvent->source.reset(source);

	kernelEvent->setEventWaitList(num_events_in_wait_list, event_wait_list);
	cl_int ret_val = commandQueue->enqueueEvent(kernelEvent);

	if(ret_val == CL_SUCCESS)
	{
		if(event != NULL)
			//copy event to output
			*event = kernelEvent->toBase();
	}
	else
		ignoreReturnValue(kernelEvent->release(), __FILE__, __LINE__, "At this point, this method already failed");

	return ret_val;
}

/*!
 * OpenCL 1.2 specification, pages 158+:
 *
 *  \param program is a program object with a successfully built executable.
 *
 *  \param kernel_name is a function name in the program declared with the __kernel qualifier.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateKernel returns a valid non-zero kernel object and errcode_ret is set to CL_SUCCESS if the kernel object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built executable for program .
 *  - CL_INVALID_KERNEL_NAME if kernel_name is not found in program.
 *  - CL_INVALID_KERNEL_DEFINITION if the function definition for __kernel function given by kernel_name such as the number of arguments,
 *  the argument types are not the same for all devices for which the program executable has been built.
 *  - CL_INVALID_VALUE if kernel_name is NULL .
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_kernel VC4CL_FUNC(clCreateKernel)(cl_program program, const char* kernel_name, cl_int* errcode_ret)
{
	CHECK_PROGRAM_ERROR_CODE(toType<Program>(program), errcode_ret, cl_kernel)

	if(toType<Program>(program)->kernelInfo.empty())
		return returnError<cl_kernel>(CL_INVALID_PROGRAM_EXECUTABLE, errcode_ret, __FILE__, __LINE__, "Program has no kernel-info, may not be compiled!");

	if(kernel_name == NULL)
		return returnError<cl_kernel>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No kernel-name was set!");

	const KernelInfo* info = nullptr;
	for(const KernelInfo& i : toType<Program>(program)->kernelInfo)
	{
		if(i.name.compare(kernel_name) == 0)
		{
			info = &i;
			break;
		}
	}
	if(info == nullptr)
		return returnError<cl_kernel>(CL_INVALID_KERNEL_NAME, errcode_ret, __FILE__, __LINE__, buildString("Failed to retrieve info for kernel %s!", kernel_name));

	Kernel* kernel = newObject<Kernel>(toType<Program>(program), *info);
	CHECK_ALLOCATION_ERROR_CODE(kernel, errcode_ret, cl_kernel)
	RETURN_OBJECT(kernel->toBase(), errcode_ret);
}

/*!
 * OpenCL 1.2 specification, pages 159+:
 *
 *  Creates kernel objects for all kernel functions in program. Kernel objects are not created for any __kernel functions in program that do not have the
 *  same function definition across all devices for which a program executable has been successfully built.
 *
 *  \param program is a program object with a successfully built executable.
 *
 *  \param num_kernels is the size of memory pointed to by kernels specified as the number of cl_kernel entries.
 *
 *  \param kernels is the buffer where the kernel objects for kernels in program will be returned. If kernels is NULL , it is ignored.
 *  If kernels is not NULL , num_kernels must be greater than or equal to the number of kernels in program.
 *
 *  \param num_kernels_ret is the number of kernels in program. If num_kernels_ret is NULL , it is ignored.
 *
 *  \return clCreateKernelsInProgram will return CL_SUCCESS if the kernel objects were successfully allocated. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built executable for any device in program.
 *  - CL_INVALID_VALUE if kernels is not NULL and num_kernels is less than the number of kernels in program.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  Kernel objects can only be created once you have a program object with a valid program source or binary loaded into the program object and the program executable
 *  has been successfully built for one or more devices associated with program. No changes to the program executable are allowed while there are kernel objects associated with a program object.
 *  This means that calls to clBuildProgram and clCompileProgram return CL_INVALID_OPERATION if there are kernel objects attached to a program object.
 *  The OpenCL context associated with program will be the context associated with kernel. The list of devices associated with program are the devices associated with kernel.
 *  Devices associated with a program object for which a valid program executable has been built can be used to execute kernels declared in the program object.
 */
cl_int VC4CL_FUNC(clCreateKernelsInProgram)(cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret)
{
	CHECK_PROGRAM(toType<Program>(program))

		if(toType<Program>(program)->kernelInfo.empty())
		return returnError(CL_INVALID_PROGRAM_EXECUTABLE, __FILE__, __LINE__, "No kernel-info found, maybe program was not yet compiled!");

	if(kernels != NULL && num_kernels < toType<Program>(program)->kernelInfo.size())
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Ouput parameter cannot hold all %d kernels", toType<Program>(program)->kernelInfo.size()));

	size_t i = 0;
	for(const KernelInfo& info : toType<Program>(program)->kernelInfo)
	{
		//if kernels is NULL, kernels are created but not referenced -> they leak!!
		if(kernels != NULL)
		{
			Kernel* k = newObject<Kernel>(toType<Program>(program), info);
			CHECK_ALLOCATION(k)
			kernels[i] = k->toBase();
		}
		++i;
	}

	if(num_kernels_ret != NULL)
		*num_kernels_ret = toType<Program>(program)->kernelInfo.size();

	return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, page 160:
 *
 *  Increments the kernel reference count.
 *
 *  \return clRetainKernel returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_KERNEL if kernel is not a valid kernel object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  clCreateKernel or clCreateKernelsInProgram do an implicit retain.
 */
cl_int VC4CL_FUNC(clRetainKernel)(cl_kernel kernel)
{
	CHECK_KERNEL(toType<Kernel>(kernel))
	return toType<Kernel>(kernel)->retain();
}

/*!
 * OpenCL 1.2 specification, page 160:
 *
 *  Decrements the kernel reference count.
 *
 *  \return clReleaseKernel returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_KERNEL if kernel is not a valid kernel object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  The kernel object is deleted once the number of instances that are retained to kernel become zero and the kernel object is no longer needed by any enqueued commands that use kernel.
 */
cl_int VC4CL_FUNC(clReleaseKernel)(cl_kernel kernel)
{
	CHECK_KERNEL(toType<Kernel>(kernel))
	return toType<Kernel>(kernel)->release();
}

/*!
 * OpenCL 1.2 specification, pages 161+:
 *
 *  Is used to set the argument value for a specific argument of a kernel.
 *
 *  \param kernel is a valid kernel object.
 *
 *  \param arg_index is the argument index. Arguments to the kernel are referred by indices that go from 0 for the leftmost argument to n - 1, where n is the total number of arguments declared by a kernel.
 *
 *  \param arg_value is a pointer to data that should be used as the argument value for argument specified by arg_index.
 *  The argument data pointed to by arg_value is copied and the arg_value pointer can therefore be reused by the application after clSetKernelArg returns.
 *  The argument value specified is the value used by all API calls that enqueue kernel (clEnqueueNDRangeKernel and clEnqueueTask) until the argument value is changed by a call to clSetKernelArg for kernel.
 *
 *  If the argument is a memory object (buffer, image or image array), the arg_value entry will be a pointer to the appropriate buffer, image or image array object.
 *  The memory object must be created with the context associated with the kernel object. If the argument is a buffer object, the arg_value pointer can be NULL or point to a NULL value in which
 *  case a NULL value will be used as the value for the argument declared as a pointer to __global or __constant memory in the kernel. If the argument is declared with the __local qualifier,
 *  the arg_value entry must be NULL . If the argument is of type sampler_t, the arg_value entry must be a pointer to the sampler object.
 *
 *  If the argument is declared to be a pointer of a built-in scalar or vector type, or a user defined structure type in the global or constant address space,
 *  the memory object specified as argument value must be a buffer object (or NULL ). If the argument is declared with the __constant qualifier,
 *  the size in bytes of the memory object cannot exceed CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE and the number of arguments declared as pointers to __constant memory cannot exceed CL_DEVICE_MAX_CONSTANT_ARGS .
 *
 *  The memory object specified as argument value must be a 2D image object if the argument is declared to be of type image2d_t. The memory object specified as argument value must be a 3D
 *  image object if argument is declared to be of type image3d_t. The memory object specified as argument value must be a 1D image object if the argument is declared to be of type image1d_t.
 *  The memory object specified as argument value must be a 1D image buffer object if the argument is declared to be of type image1d_buffer_t. The memory object specified as argument value must be a 1D image array object if argument is declared to be of type image1d_array_t.
 *  The memory object specified as argument value must be a 2D image array object if argument is declared to be of type image2d_array_t.
 *  For all other kernel arguments, the arg_value entry must be a pointer to the actual data to be used as argument value.
 *
 *  \param arg_size specifies the size of the argument value. If the argument is a memory object, the size is the size of the buffer or image object type. For arguments declared with the __local qualifier,
 *  the size specified will be the size in bytes of the buffer that must be allocated for the __local argument. If the argument is of type sampler_t, the arg_size value must be equal to sizeof(cl_sampler).
 *  For all other arguments, the size will be the size of argument type.
 *
 *  NOTE: A kernel object does not update the reference count for objects such as memory, sampler objects specified as argument values by clSetKernelArg,
 *  Users may not rely on a kernel object to retain objects specified as argument values to the kernel.
 *
 *  \return clSetKernelArg returns CL_SUCCESS if the function was executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_KERNEL if kernel is not a valid kernel object.
 *  - CL_INVALID_ARG_INDEX if arg_index is not a valid argument index.
 *  - CL_INVALID_ARG_VALUE if arg_value specified is not a valid value.
 *  - CL_INVALID_MEM_OBJECT for an argument declared to be a memory object when the specified arg_value is not a valid memory object.
 *  - CL_INVALID_SAMPLER for an argument declared to be of type sampler_t when the specified arg_value is not a valid sampler object.
 *  - CL_INVALID_ARG_SIZE if arg_size does not match the size of the data type for an argument that is not a memory object or if the argument is a memory object and arg_size != sizeof(cl_mem) or if arg_size is zero and the argument is declared with the
 *  __local qualifier or if the argument is a sampler and arg_size != sizeof(cl_sampler).
 *  - CL_INVALID_ARG_VALUE if the argument is an image declared with the read_only qualifier and arg_value refers to an image object created with cl_mem_flags of CL_MEM_WRITE or if the image argument is declared with the write_only qualifier
 *  and arg_value refers to an image object created with cl_mem_flags of CL_MEM_READ .
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clSetKernelArg)(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value)
{
	CHECK_KERNEL(toType<Kernel>(kernel))
	return toType<Kernel>(kernel)->setArg(arg_index, arg_size, arg_value);
}

/*!
 * OpenCL 1.2 specification, pages 163+:
 *
 *  Returns information about the kernel object.
 *
 *  \param kernel specifies the kernel object being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information returned in param_value by clGetKernelInfo is described in table 5.15.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be >= size of return type as described in table 5.15.
 *
 *  \return clGetKernelInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return type as described in table 5.15 and param_value is not NULL .
 *  - CL_INVALID_KERNEL if kernel is a not a valid kernel object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clGetKernelInfo)(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_KERNEL(toType<Kernel>(kernel))
	return toType<Kernel>(kernel)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

/*!
 * OpenCL 1.2 specification, pages  165+:
 *
 *  Returns information about the kernel object that may be specific to a device.
 *
 *  \param kernel specifies the kernel object being queried.
 *
 *  \param device identifies a specific device in the list of devices associated with kernel. The list of devices is the list of devices in the OpenCL context that is associated with kernel.
 *  If the list of devices associated with kernel is a single device, device can be a NULL value.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information returned in param_value by clGetKernelWorkGroupInfo is described in table 5.16.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be >= size of return type as described in table 5.16.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetKernelWorkGroupInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_DEVICE if device is not in the list of devices associated with kernel or if device is NULL but there is more than one device associated with kernel.
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return type as described in table 5.15 and param_value is not NULL .
 *  - CL_INVALID_VALUE if param_name is CL_KERNEL_GLOBAL_WORK_SIZE and device is not a custom device or kernel is not a built-in kernel.
 *  - CL_INVALID_KERNEL if kernel is a not a valid kernel object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clGetKernelWorkGroupInfo)(cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_KERNEL(toType<Kernel>(kernel))
	if(device == NULL)
			device = const_cast<cl_device_id>(toType<Kernel>(kernel)->program->context()->device->toBase());
	CHECK_DEVICE_WITH_CONTEXT(toType<Device>(device), toType<Kernel>(kernel)->program->context())
	return toType<Kernel>(kernel)->getWorkGroupInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

/*!
 * OpenCL 1.2 specification, pages 168+:
 *
 *  Returns information about the arguments of a kernel. Kernel argument information is only available if the program object associated with kernel is created with clCreateProgramWithSource and the program executable is built with the -cl-kernel-arg-info option specified in options argument to clBuildProgram or clCompileProgram.
 *
 *  \param kernel specifies the kernel object being queried.
 *
 *  \param arg_indx is the argument index. Arguments to the kernel are referred by indices that go from 0 for the leftmost argument to n - 1, where n is the total number of arguments declared by a kernel.
 *
 *  \param param_name specifies the argument information to query. The list of supported param_name types and the information returned in param_value by clGetKernelArgInfo is described in table 5.17.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be > size of return type as described in table 5.17.
 *
 *  \param param_value_size ret returns the actual size in bytes of data copied to param value. If param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetKernelArgInfo returns CL SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_ARG_INDEX if arg_indx is not a valid argument index.
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value size is < size of return type as described in table 5.17 and param_value is not NULL .
 *  - CL_KERNEL_ARG_INFO_NOT_AVAILABLE if the argument information is not available for kernel.
 *  - CL_INVALID_KERNEL if kernel is a not a valid kernel object.
 */
cl_int VC4CL_FUNC(clGetKernelArgInfo)(cl_kernel kernel, cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_KERNEL(toType<Kernel>(kernel))
	return toType<Kernel>(kernel)->getArgInfo(arg_index, param_name, param_value_size, param_value, param_value_size_ret);
}

/*!
 * OpenCL 1.2 specification, pages 171+:
 *
 *  Enqueues a command to execute a kernel on a device.
 *
 *  \param command_queue is a valid command-queue. The kernel will be queued for execution on the device associated with command_queue.
 *
 *  \param kernel is a valid kernel object. The OpenCL context associated with kernel and command-queue must be the same.
 *
 *  \param work_dim is the number of dimensions used to specify the global work-items and work-items in the work-group. work_dim must be greater than zero and less than or equal to CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS .
 *
 *  \param global_work_offset can be used to specify an array of work_dim unsigned values that describe the offset used to calculate the global ID of a work-item.
 *  If global_work_offset is NULL , the global IDs start at offset (0, 0, ... 0).
 *
 *  \param global_work_size points to an array of work_dim unsigned values that describe the number of global work-items in work_dim dimensions that will execute the kernel function.
 *  The total number of global work-items is computed as global_work_size[0] * ... * global_work_size[work_dim – 1].
 *
 *  \param local_work_size points to an array of work_dim unsigned values that describe the number of work-items that make up a work-group (also referred to as the size of the work-group)
 *  that will execute the kernel specified by kernel. The total number of work-items in a work-group is computed as local_work_size[0] * ... * local_work_size[work_dim – 1].
 *  The total number of work-items in the work-group must be less than or equal to the CL_DEVICE_MAX_WORK_GROUP_SIZE value specified in table 4.3 and the number of work-items specified in local_work_size[0], ... local_work_size[work_dim – 1]
 *  must be less than or equal to the corresponding values specified by CL_DEVICE_MAX_WORK_ITEM_SIZES [0], .... CL_DEVICE_MAX_WORK_ITEM_SIZES [work_dim – 1].
 *  The explicitly specified local_work_size will be used to determine how to break the global work-items specified by
 *
 *  \param global_work_size into appropriate work-group instances. If local_work_size is specified, the values specified in global_work_size[0], ... global_work_size[work_dim - 1] must be evenly
 *  divisible by the corresponding values specified in local_work_size[0], ... local_work_size[work_dim – 1].
 *
 *  The work-group size to be used for kernel can also be specified in the program source using the __attribute__((reqd_work_group_size(X, Y, Z)))qualifier (refer to section 6.7.2).
 *  In this case the size of work group specified by local_work_size must match the value specified by the reqd_work_group_size attribute qualifier.
 *  local_work_size can also be a NULL value in which case the OpenCL implementation will determine how to be break the global work-items into appropriate work-group instances.
 *  These work-group instances are executed in parallel across multiple compute units or concurrently on the same compute unit.
 *
 *  Each work-item is uniquely identified by a global identifier. The global ID, which can be read inside the kernel, is computed using the value given by global_work_size and
 *  global_work_offset. In addition, a work-item is also identified within a work-group by a unique local ID. The local ID, which can also be read by the kernel,
 *  is computed using the value given by local_work_size. The starting local ID is always (0, 0, ... 0).
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular command can be executed. If event_wait_list is NULL ,
 *  then this particular command does not wait on any event to complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL ,
 *  the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events specified in event_wait_list act as synchronization points.
 *  The context associated with events in event_wait_list and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular kernel execution instance. Event objects are unique and can be used to identify a particular kernel execution instance later on.
 *  If event is NULL , no event will be created for this kernel execution instance and therefore it will not be possible for the application to query or queue a wait for this particular kernel execution
 *  instance. If the event_wait_list and the event arguments are not NULL , the event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueNDRangeKernel returns CL_SUCCESS if the kernel execution was successfully queued. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built program executable available for device associated with command_queue.
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_KERNEL if kernel is not a valid kernel object.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and kernel are not the same or if the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_KERNEL_ARGS if the kernel argument values have not been specified.
 *  - CL_INVALID_WORK_DIMENSION if work_dim is not a valid value (i.e. a value between 1 and 3).
 *  - CL_INVALID_GLOBAL_WORK_SIZE if global_work_size is NULL , or if any of the values specified in global_work_size[0], ... global_work_size[work_dim – 1] are 0 or exceed the range given by the sizeof(size_t) for the device on which the kernel execution will be enqueued.
 *  - CL_INVALID_GLOBAL_OFFSET if the value specified in global_work_size + the corresponding values in global_work_offset for any dimensions is greater than the sizeof(size t) for the device on which the kernel execution will be enqueued.
 *  - CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and number of work-items specified by global_work_size is not evenly divisible by size of work-group given by local_work_size or does not match the work-group size specified for kernel using the
 *  __attribute__((reqd_work_group_size(X, Y, Z))) qualifier in program source.
 *  - CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and the total number of work-items in the work-group computed as local_work_size[0] * ... local_work_size[work_dim – 1] is greater than the value specified by
 *  CL_DEVICE_MAX_WORK_GROUP_SIZE in table 4.3.
 *  - CL_INVALID_WORK_GROUP_SIZE if local_work_size is NULL and the __attribute__((reqd_work_group_size(X, Y, Z))) qualifier is used to declare the work-group size for kernel in the program source.
 *  - CL_INVALID_WORK_ITEM_SIZE if the number of work-items specified in any of local_work_size[0], ... local_work_size[work_dim – 1] is greater than the corresponding values specified by CL_DEVICE_MAX_WORK_ITEM_SIZES [0], ....
 *  CL_DEVICE_MAX_WORK_ITEM_SIZES [work_dim – 1].
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if a sub-buffer object is specified as the value for an argument that is a buffer object and the offset specified when the sub-buffer object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
 *  associated with queue.
 *  - CL_INVALID_IMAGE_SIZE if an image object is specified as an argument value and the image dimensions (image width, height, specified or compute row and/or slice pitch) are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if an image object is specified as an argument value and the image format (image channel order and data type) is not supported by device associated with queue.
 *  - CL_OUT_OF_RESOURCES if there is a failure to queue the execution instance of kernel on the command-queue because of insufficient resources needed to execute the kernel.
 *  For example, the explicitly specified local_work_size causes a failure to execute the kernel because of insufficient resources such as registers or local memory.
 *  Another example would be the number of read-only image args used in kernel exceed the CL_DEVICE_MAX_READ_IMAGE_ARGS value for device or the number of write-only image args used in kernel exceed the CL_DEVICE_MAX_WRITE_IMAGE_ARGS value for device
 *  or the number of samplers used in kernel exceed CL_DEVICE_MAX_SAMPLERS for device.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with image or buffer objects specified as arguments to kernel.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clEnqueueNDRangeKernel)(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_KERNEL(toType<Kernel>(kernel))

	return toType<Kernel>(kernel)->enqueueNDRange(toType<CommandQueue>(command_queue), work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 174+:
 *
 *  Enqueues a command to execute a kernel on a device. The kernel is executed using a single work-item.
 *
 *  \param command_queue is a valid command-queue. The kernel will be queued for execution on the device associated with command_queue.
 *
 *  \param kernel is a valid kernel object. The OpenCL context associated with kernel and command-queue must be the same.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular command can be executed. If event_wait_list is NULL ,
 *  then this particular command does not wait on any event to complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL ,
 *  the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0.
 *  The events specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and command_queue must be the same.
 *  The memory associated with event_wait_list can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular kernel execution instance. Event objects are unique and can be used to identify a particular kernel execution instance later on.
 *  If event is NULL , no event will be created for this kernel execution instance and therefore it will not be possible for the application to query or queue a wait for this particular kernel execution
 *  instance. If the event_wait_list and the event arguments are not NULL , the event argument should not refer to an element of the event_wait_list array.
 *
 *  clEnqueueTask is equivalent to calling clEnqueueNDRangeKernel with work_dim = 1, global_work_offset = NULL , global_work_size[0] set to 1 and local_work_size[0] set to 1.
 *
 *  \return clEnqueueTask returns CL_SUCCESS if the kernel execution was successfully queued. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built program executable available for device associated with command_queue.
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_KERNEL if kernel is not a valid kernel object.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and kernel are not the same or if the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_KERNEL_ARGS if the kernel argument values have not been specified.
 *  - CL_INVALID_WORK_GROUP_SIZE if a work-group size is specified for kernel using the __attribute__((reqd_work_group_size(X, Y, Z))) qualifier in program source and is not (1, 1, 1).
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if a sub-buffer object is specified as the value for an argument that is a buffer object and the offset specified when the sub-buffer object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_INVALID_IMAGE_SIZE if an image object is specified as an argument value and the image dimensions (image width, height, specified or compute row and/or slice pitch) are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if an image object is specified as an argument value and the image format (image channel order and data type) is not supported by device associated with queue.
 *  - CL_OUT_OF_RESOURCES if there is a failure to queue the execution instance of kernel on the command-queue because of insufficient resources needed to execute the kernel.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with image or buffer objects specified as arguments to kernel.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clEnqueueTask)(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	//"clEnqueueTask is equivalent to calling clEnqueueNDRangeKernel with work_dim = 1, global_work_offset = NULL,
	// global_work_size[0] set to 1 and local_work_size[0] set to 1."
	const size_t work_size = 1;
	return VC4CL_FUNC(clEnqueueNDRangeKernel)(command_queue, kernel, 1, NULL, &work_size, &work_size, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 176+:
 *
 *  Enqueues a command to execute a native C/C++ function not compiled using the OpenCL compiler.
 *
 *  \param command_queue is a valid command-queue. A native user function can only be executed on a command-queue created on a device that has CL_EXEC_NATIVE_KERNEL capability set in
 *  CL_DEVICE_EXECUTION_CAPABILITIES as specified in table 4.3. user_func is a pointer to a host-callable user function.
 *
 *  \param args is a pointer to the args list that user_func should be called with.
 *
 *  \param cb_args is the size in bytes of the args list that args points to. The data pointed to by args and cb_args bytes in size will be copied and a pointer to this copied
 *  region will be passed to user_func. The copy needs to be done because the memory objects (cl_mem values) that args may contain need to be modified and replaced by appropriate
 *  pointers to global memory. When clEnqueueNativeKernel returns, the memory region pointed to by args can be reused by the application.
 *
 *  \param num_mem_objects is the number of buffer objects that are passed in args.
 *
 *  \param mem_list is a list of valid buffer objects, if num_mem_objects > 0. The buffer object values specified in mem_list are memory object handles (cl_mem values) returned by clCreateBuffer
 *  or NULL .
 *
 *  \param args_mem_loc is a pointer to appropriate locations that args points to where memory object handles (cl_mem values) are stored. Before the user function is executed, the memory object
 *  handles are replaced by pointers to global memory.
 *
 *  \param event_wait_list, num_events_in_wait_list and event are as described in clEnqueueNDRangeKernel.
 *
 *  \return clEnqueueNativeKernel returns CL_SUCCESS if the user function execution instance was successfully queued. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_VALUE if user_func is NULL .
 *  - CL_INVALID_VALUE if args is a NULL value and cb_args > 0, or if args is a NULL value and num_mem_objects > 0.
 *  - CL_INVALID_VALUE if args is not NULL and cb_args is 0.
 *  - CL_INVALID_VALUE if num_mem_objects > 0 and mem_list or args_mem_loc are NULL .
 *  - CL_INVALID_VALUE if num_mem_objects = 0 and mem_list or args_mem_loc are not NULL .
 *  - CL_INVALID_OPERATION if the device associated with command_queue cannot execute the native kernel.
 *  - CL_INVALID_MEM_OBJECT if one or more memory objects specified in mem_list are not valid or are not buffer objects.
 *  - CL_OUT_OF_RESOURCES if there is a failure to queue the execution instance of kernel on the command-queue because of insufficient resources needed to execute the kernel.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer objects specified as arguments to kernel.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  NOTE: The total number of read-only images specified as arguments to a kernel cannot exceed CL_DEVICE_MAX_READ_IMAGE_ARGS . Each 2D image array argument to a kernel declared
 *  with the read_only qualifier counts as one image. The total number of write-only images specified as arguments to a kernel cannot exceed CL_DEVICE_MAX_WRITE_IMAGE_ARGS .
 *  Each 2D image array argument to a kernel declared with the write_only qualifier counts as one image.
 */
cl_int VC4CL_FUNC(clEnqueueNativeKernel)(cl_command_queue command_queue, void(CL_CALLBACK* user_func)(void *), void* args, size_t cb_args, cl_uint num_mem_objects, const cl_mem* mem_list, const void** args_mem_loc, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

	//no native kernels are supported
	return CL_INVALID_OPERATION;
}

/*!
 * OpenCL 2.0 specification, pages 218+:
 *  Sets a SVM pointer as the argument value for a specific argument of a kernel.
 *
 *  \param kernel is a valid kernel object.
 *
 *  \param arg_index is the argument index.  Arguments to the kernel are referred by indices that go from 0 for the leftmost argument to n-1, where n is the total number of arguments declared by a kernel.
 *
 *  \param arg_value is the SVM pointer that should be used as the argument value for argument specified by arg_index.
 *  The SVM pointer specified is the value used by all API calls that enqueue kernel(clEnqueueNDRangeKernel) until the argument value is changed by a call to clSetKernelArgSVMPointer for kernel.
 *  The SVM pointer can only be used for arguments that are declared to be a pointer to global or constant memory. The SVM pointer value must be aligned according to the argument’s type.
 *  For example, if the argument is declared to be global float4 *p, the SVM pointer value passed for p must be at a minimum aligned to a float4.
 *  The SVM pointer value specified as the argument value can be the pointer returned by clSVMAlloc or can be a pointer + offset into the SVM region.
 *
 *  \return clSetKernelArgSVMPointer returns CL_SUCCESS if the function was executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_KERNEL if kernel is not a valid kernel object.
 *  - CL_INVALID_ARG_INDEX if arg_index is not a valid argument index.
 *  - CL_INVALID_ARG_VALUE if arg_value specified is not a valid value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clSetKernelArgSVMPointerARM)(cl_kernel kernel, cl_uint arg_index, const void* arg_value)
{
	CHECK_KERNEL(toType<Kernel>(kernel))

	SharedVirtualMemory* svm = SharedVirtualMemory::findSVM(arg_value);
	if(svm == nullptr)
		return returnError(CL_INVALID_ARG_VALUE, __FILE__, __LINE__, "SVM pointer is NULL");
	cl_int offset = svm->getHostOffset(arg_value);
	if(offset < 0)
		return returnError(CL_INVALID_ARG_VALUE, __FILE__, __LINE__, "SVM pointer does not point to any SVM object");

	return toType<Kernel>(kernel)->setArg(arg_index, sizeof(void*), svm->getDevicePointer(offset), true);
}

/*!
 * OpenCL 2.0 specification, pages 219+:
 *  Pass additional information other than argument values to a kernel.
 *
 *  \param kernel specifies the kernel object being queried.
 *
 *  \param param_name specifies the information to be passed to kernel. The list of supported param_name types and the corresponding values passed in param_value is described in table 5.19
 *
 *  \param param_value_size specifies the size in bytes of the memory pointed to by param_value.
 *
 *  \param param_value is a pointer to memory where the appropriate values determined by param_name are specified.
 *
 *  \return clSetKernelExecInfo returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of the following errors:
 *  - CL_INVALID_KERNEL if kernel is a not a valid kernel object.
 *  - CL_INVALID_VALUE if param_name is not valid, if param_value is NULL or if the size specified by param_value_size is not valid.
 *  - CL_INVALID_OPERATION if param_name = CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM and param_value = CL_TRUE but no devices in context associated with kernel support fine-grain system SVM allocations.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  NOTES
 *  1. Coarse-grain or fine-grain buffer SVM pointers used by a kernel which are not passed as a kernel arguments must be specified using clSetKernelExecInfo with CL_KERNEL_EXEC_INFO_SVM_PTRS.
 *  For example, if SVM buffer A contains a pointer to another SVM buffer B, and the kernel dereferences that pointer, then a pointer to B must either be passed as an argument in the call to that kernel or it must be made available to the kernel
 *  using clSetKernelExecInfo. When calling clSetKernelExecInfo with CL_KERNEL_EXEC_INFO_SVM_PTRS to specify pointers to non-argument SVM buffers as extra arguments to a kernel,
 *  each of these pointers can be the SVM pointer returned by clSVMAlloc or can be a pointer + offset into the SVM region.It is sufficient to provide one pointer for each SVM buffer used.
 *
 *  2. CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM is used to indicate whether SVM pointers used by a kernel will refer to system allocations or not.
 *  CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM = CL_FALSE indicates that the OpenCL implementation may assume that system pointers are not passed as kernel arguments and are not stored inside SVM allocations passed as kernel arguments.
 *  CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM = CL_TRUE indicates that the OpenCL implementation must assume that system pointers might be passed as kernel arguments and/or stored inside SVM allocations passed as kernel arguments.
 *  In this case, if the device to which the kernel is enqueued does not support system SVM pointers, clEnqueueNDRangeKernel will return a CL_INVALID_OPERATION error.
 *  If none of the devices in the context associated with kernel support fine-grain system SVM allocations, clSetKernelExecInfo will return a CL_INVALID_OPERATION error.
 *
 *  If clSetKernelExecInfo has not been called with a value for CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, the default value is used for this kernel attribute.  The default value depends on whether the device on which the kernel is
 *  enqueued supports fine-grain system SVM allocations.  If so, the default value used is CL_TRUE (system pointers might be passed); otherwise, the default is CL_FALSE.
 *
 *  3. A call to clSetKernelExecInfo for a given value of param_name replaces any prior value passed for that value of param_name. Only one param_value will be stored for each value of param_name.
 */
cl_int VC4CL_FUNC(clSetKernelExecInfoARM)(cl_kernel kernel, cl_kernel_exec_info_arm param_name, size_t param_value_size, const void* param_value)
{
	CHECK_KERNEL(toType<Kernel>(kernel))

	//CL_KERNEL_EXEC_INFO_SVM_PTRS sets "pointers used by a kernel which are not passed as a kernel arguments"
	//All kernels already can access all buffers
	//CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM set "whether the kernel uses pointers that are fine grain system SVM allocations"

	if(param_name != CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM && param_name != CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM_ARM)
		return CL_INVALID_VALUE;
	if(param_name == CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM_ARM && *((cl_bool*)param_value) == CL_TRUE)
		//we do not support system allocations!
		return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "System SVM allocations are not supported");

	//TODO we need to convert host-pointer into device-pointer, for every pointer in buffers!!
	return CL_INVALID_OPERATION;
}
