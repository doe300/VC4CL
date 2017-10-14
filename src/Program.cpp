/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <sstream>
#include <iterator>

#include "Program.h"
#include "Device.h"
#include "extensions.h"
#include "V3D.h"

#ifdef COMPILER_HEADER
#define CPPLOG_NAMESPACE logging
    #include COMPILER_HEADER
#endif

using namespace vc4cl;

/*
 * TODO rewrite, so compile is OpenCL C -> SPIR-V/LLVM-IR and link is SPIR-V/LLVM-IR -> asm
 * + can use linker in SPIR-V Tools to support multiple input programs
 * + more accurate wrt meanings of compilation and linking
 * - need extra buffer for intermediate code
 */

size_t KernelInfo::getExplicitUniformCount() const
{
	size_t count = 0;
	for(const ParamInfo& info : params)
		count += info.elements;
	return count;
}

Program::Program(Context* context, const std::vector<char>& code, const cl_bool isBinary) : HasContext(context)
{
	if(isBinary)
		binaryCode = code;
	else
		sourceCode = code;
}

Program::~Program()
{
}

#if HAS_COMPILER
static cl_int compile_program(Program* program, const std::string& options)
{
	std::istringstream sourceCode;
	sourceCode.str(std::string(program->sourceCode.data(), program->sourceCode.size()));

	vc4c::SourceType sourceType = vc4c::Precompiler::getSourceType(sourceCode);
	if(sourceType == vc4c::SourceType::UNKNOWN || sourceType == vc4c::SourceType::QPUASM_BIN || sourceType == vc4c::SourceType::QPUASM_HEX)
		return returnError(CL_COMPILE_PROGRAM_FAILURE, __FILE__, __LINE__, buildString("Invalid source-code type %d", sourceType));

	vc4c::Configuration config;
	//set the configuration for the available VPM size
	//XXX total VPM size or only user size?
	config.availableVPMSize = V3D::instance().getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);

#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Compiling source with: "<< program->buildInfo.options << std::endl;
#endif

	std::wstringstream logStream;
	try
	{
		std::stringstream binaryCode;
		vc4c::setLogger(logStream, false, vc4c::LogLevel::WARNING);
		program->buildInfo.options = options;
		std::size_t numBytes = vc4c::Compiler::compile(sourceCode, binaryCode, config, options);
		program->buildInfo.status = CL_SUCCESS;
		program->binaryCode.resize(numBytes, '\0');

		memcpy(program->binaryCode.data(), binaryCode.str().data(), numBytes);

	}
	catch(vc4c::CompilationError& e)
	{
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Compilation error: " << e.what() << std::endl;
		program->buildInfo.status = CL_BUILD_ERROR;
#endif
	}
	//copy log whether build failed or not
	//this method s not supported by the Raspbian GCC
	//std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> logConverter;
	//program->buildInfo.log = logConverter.to_bytes(logStream.str());
	//XXX alternatively could use std::wcstombs
	program->buildInfo.log = std::string((const char*)logStream.str().data());

#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Compilation complete with status: " << program->buildInfo.status << std::endl;
	if(!program->buildInfo.log.empty())
		std::cout << "[VC4CL] Compilation log: " << program->buildInfo.log << std::endl;
#endif

	return program->buildInfo.status;
}
#endif

cl_int Program::compile(const std::string& options, BuildCallback callback, void* userData)
{
	if(sourceCode.empty())
		return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "There is no source code to compile!");
	//if the program was already compiled, clear all results
	binaryCode.clear();
	kernelInfo.clear();
#if HAS_COMPILER
	buildInfo.status = CL_BUILD_IN_PROGRESS;
	cl_int state = compile_program(this, options);
	if(callback != NULL)
		(callback)(toBase(), userData);
#else
	buildInfo.status = CL_BUILD_NONE;
	cl_int state = CL_COMPILER_NOT_AVAILABLE;
#endif
	return state;
}

cl_int Program::link(const std::string& options, BuildCallback callback, void* userData)
{
	if(binaryCode.empty())
		//not yet compiled
		return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Program needs to be compiled first!");
	//if the program was already compiled, clear all results
	kernelInfo.clear();

	// extract kernel-info
	cl_ulong* ptr = (cl_ulong*)binaryCode.data();
	ptr += 1;	//skips magic number
	//the minimum offset for the first kernel-function
	cl_uint min_kernel_offset = UINT32_MAX;
	while(ptr[0] != 0)
	{
		cl_int state = extractKernelInfo(&ptr, &min_kernel_offset);
		if(state != CL_SUCCESS)
		{
			kernelInfo.clear();
			return state;
		}
	}

	ptr += 1; //skips the 0-long as marker between header and data
	if((cl_ulong*)(binaryCode.data() + (min_kernel_offset - 1) * sizeof(cl_ulong)) != ptr)
	{
		//if the pointer to the second next instruction after the header is not the pointer to the first kernel, there are global data
		cl_uchar* global_data_ptr = (cl_uchar*)ptr;
		const size_t globalDataSize = (cl_uchar*)(binaryCode.data() + (min_kernel_offset - 1) * sizeof(cl_ulong)) - global_data_ptr;
		globalData.reserve(globalDataSize);

		std::copy(global_data_ptr, global_data_ptr + globalDataSize, std::back_inserter(globalData));
	}

	if(callback != NULL)
		(callback)(toBase(), userData);

	return CL_SUCCESS;
}

cl_int Program::getInfo(cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	std::string kernelNames;
	for(const KernelInfo& info : kernelInfo)
	{
		kernelNames.append(info.name).append(";");
	}
	//remove last semicolon
	kernelNames = kernelNames.substr(0, kernelNames.length() - 1);

	switch(param_name)
	{
		case CL_PROGRAM_REFERENCE_COUNT:
			return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_CONTEXT:
			return returnValue<cl_context>(context()->toBase(), param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_NUM_DEVICES:
			return returnValue<cl_uint>(1, param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_DEVICES:
		{
			return returnValue<cl_device_id>(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), param_value_size, param_value, param_value_size_ret);
		}
		case CL_PROGRAM_IL_KHR:
			//"Returns the program IL for programs created with clCreateProgramWithILKHR"
			//TODO only return source, if created with clCreateProgramWithILKHR
		case CL_PROGRAM_SOURCE:
			if(sourceCode.empty())
				return returnString("", param_value_size, param_value, param_value_size_ret);
			return returnValue(sourceCode.data(), sizeof(char), sourceCode.size(), param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_BINARY_SIZES:
		{
			return returnValue<size_t>(binaryCode.size(), param_value_size, param_value, param_value_size_ret);
		}
		case CL_PROGRAM_BINARIES:
			//"param_value points to an array of n pointers allocated by the caller, where n is the number of devices associated with program. "
			if(binaryCode.empty())
				return returnValue(NULL, 0, 0, param_value_size, param_value, param_value_size_ret);
			return returnValue<unsigned char*>(reinterpret_cast<unsigned char*>(binaryCode.data()), param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_NUM_KERNELS:
			if(kernelInfo.empty())
				return CL_INVALID_PROGRAM_EXECUTABLE;
			return returnValue<size_t>(kernelInfo.size(), param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_KERNEL_NAMES:
			if(kernelInfo.empty())
				return CL_INVALID_PROGRAM_EXECUTABLE;
			return returnString(kernelNames, param_value_size, param_value, param_value_size_ret);
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_program_info value %d", param_name));
}

cl_int Program::getBuildInfo(cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	switch(param_name)
	{
		case CL_PROGRAM_BUILD_STATUS:
			if(binaryCode.empty())
				return returnValue<cl_build_status>(CL_BUILD_NONE, param_value_size, param_value, param_value_size_ret);
			return returnValue<cl_build_status>(buildInfo.status, param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_BUILD_OPTIONS:
			return returnString(buildInfo.options, param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_BUILD_LOG:
			return returnString(buildInfo.log, param_value_size, param_value, param_value_size_ret);
		case CL_PROGRAM_BINARY_TYPE:
			if(binaryCode.empty())
				return returnValue<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_NONE, param_value_size, param_value, param_value_size_ret);
			if(kernelInfo.empty())
				//not yet "linked"
				return returnValue<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT, param_value_size, param_value, param_value_size_ret);
			return returnValue<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_EXECUTABLE, param_value_size, param_value, param_value_size_ret);
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_program_build_info value %d", param_name));
}

BuildStatus Program::getBuildStatus() const
{
	if(binaryCode.empty())
		return BuildStatus::NOT_BUILD;
	if(kernelInfo.empty())
		return BuildStatus::COMPILED;
	return BuildStatus::DONE;
}

static std::string readString(cl_ulong** ptr, cl_uint stringLength)
{
	const std::string s((char*)*ptr, stringLength);
	*ptr += stringLength / 8;
	if(stringLength % 8 != 0)
	{
		*ptr += 1;
	}
	return s;
}

cl_int Program::extractKernelInfo(cl_ulong** ptr, cl_uint* min_kernel_offset)
{
	KernelInfo info;

	//offset|length|name-length|num-params
	info.offset = ((cl_ushort*)*ptr)[0];
	info.length = ((cl_ushort*)*ptr)[1];
	cl_uint nameLength = ((cl_ushort*)*ptr)[2];
	cl_uint num_params = ((cl_ushort*)*ptr)[3];
	*ptr += 1;
	info.compileGroupSizes[0] = **ptr & 0xFFFF;
	info.compileGroupSizes[1] = (**ptr >> 16) & 0xFFFF;
	info.compileGroupSizes[2] = (**ptr >> 32) & 0xFFFF;
	*ptr += 1;

	//name[...]|padding
	info.name = readString(ptr, nameLength);

	for(cl_ushort i = 0; i < num_params; ++i)
	{
		ParamInfo param;
		param.size = ((cl_ushort*)*ptr)[0] & 0xFF;
		param.elements = (((cl_ushort*)*ptr)[0] >> 8) & 0xFF;
		nameLength = ((cl_ushort*)*ptr)[1];
		const cl_uint typeLength = ((cl_ushort*)*ptr)[2];
		cl_ushort tmp = ((cl_ushort*)*ptr)[3];
		param.pointer = (tmp >> 12) & 0x1;
		param.output = (tmp >> 9) & 0x1;
		param.input = (tmp >> 8) & 0x1;
		param.addressSpace = static_cast<AddressSpace>((tmp >> 4) & 0xF);
		param.constant = tmp & 0x1;
		param.restricted = (tmp >> 1) & 0x1;
		param.isVolatile = (tmp >> 2) & 0x1;
		*ptr += 1;

		param.name = readString(ptr, nameLength);
		param.type = readString(ptr, typeLength);

		info.params.push_back(param);
	}

	*min_kernel_offset = std::min(*min_kernel_offset, (cl_uint)info.offset);

	kernelInfo.push_back(info);

	return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 133+:
 *
 *  Creates a program object for a context, and loads the source code specified by the text strings in the strings array into the program object.
 *  The devices associated with the program object are the devices associated with context. The source code specified by strings is either an OpenCL C program source,
 *  header or implementation-defined source for custom devices that support an online compiler.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param strings is an array of count pointers to optionally null-terminated character strings that make up the source code.
 *
 *  \param The lengths argument is an array with the number of chars in each string (the string length). If an element in lengths is zero, its accompanying string is null-terminated.
 *  If lengths is NULL , all strings in the strings argument are considered null-terminated. Any length value passed in that is greater than zero excludes the null terminator in its count.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \retrun clCreateProgramWithSource returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if the program object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if count is zero or if strings or any entry in strings is NULL .
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_program VC4CL_FUNC(clCreateProgramWithSource)(cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)

	if(count == 0 || strings == NULL)
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No source given!");
	for(cl_uint i = 0; i < count; ++i)
	{
		if(strings[i] == NULL)
			return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Source code line is NULL!");
	}

	std::vector<char> sourceCode;

	size_t code_length = 0;
	for(cl_uint i = 0; i < count; ++i)
	{
		cl_uint line_length = 0;
		if(lengths == NULL || lengths[i] == 0)
			line_length = strlen(strings[i]);
		else
			line_length = lengths[i];
		code_length += line_length;
	}
	sourceCode.reserve(code_length + 1);

	for(cl_uint i = 0; i < count; ++i)
	{
		cl_uint line_length = 0;
		if(lengths == NULL || lengths[i] == 0)
			line_length = strlen(strings[i]);
		else
			line_length = lengths[i];
		std::copy(strings[i], strings[i] + line_length, std::back_inserter(sourceCode));
	}
	sourceCode.push_back('\0');

	Program* program = newObject<Program>(toType<Context>(context), sourceCode, false);
	CHECK_ALLOCATION_ERROR_CODE(program, errcode_ret, cl_program)

	RETURN_OBJECT(program->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 extensions specification, pages 155+:
 *
 *  Creates a new program object for context using the length bytes of intermediate language pointed to by il.
 *
 *  \param context must be a valid OpenCL context
 *
 *  \param il is a pointer to a length-byte block of memory containing intermediate langage.
 *
 *  \param length is the length of the block of memory pointed to by il.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateProgramWithILKHR returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if the program object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context
 *  - CL_INVALID_VALUE if il is NULL or if length is zero
 *  - CL_INVALID_VALUE if the length-byte block of memory pointed to by il does not contain well-formed intermediate language
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host
 */
cl_program VC4CL_FUNC(clCreateProgramWithILKHR)(cl_context context, const void* il, size_t length, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)
	if(il == NULL || length == 0)
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "IL source has no length!");

	const std::vector<char> buffer((const char*)il, (const char*)il + length);
	Program* program = newObject<Program>(toType<Context>(context), buffer, false);
	CHECK_ALLOCATION_ERROR_CODE(program, errcode_ret, cl_program)

	RETURN_OBJECT(program->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 134+:
 *
 *  Creates a program object for a context, and loads the binary bits specified by binary into the program object.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param device_list is a pointer to a list of devices that are in context. device_list must be a non-NULL value.
 *  The binaries are loaded for devices specified in this list.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  The devices associated with the program object will be the list of devices specified by device_list. The list of devices specified by device_list must be devices associated with context.
 *
 *  \param lengths is an array of the size in bytes of the program binaries to be loaded for devices specified by device_list.
 *
 *  \param binaries is an array of pointers to program binaries to be loaded for devices specified by device_list.
 *  For each device given by device_list[i], the pointer to the program binary for that device is given by binaries[i] and the length of this corresponding binary is given by lengths[i].
 *  lengths[i] cannot be zero and binaries[i] cannot be a NULL pointer. The program binaries specified by binaries contain the bits that describe one of the following:
 *   - a program executable to be run on the device(s) associated with context,
 *   - a compiled program for device(s) associated with context, or
 *   - a library of compiled programs for device(s) associated with context.
 *  The program binary can consist of either or both:
 *   - Device-specific code and/or,
 *   - Implementation-specific intermediate representation (IR) which will be converted to th edevice-specific code.
 *
 *  \param binary_status returns whether the program binary for each device specified in device_list was loaded successfully or not.
 *  It is an array of num_devices entries and returns CL_SUCCESS in binary_status[i] if binary was successfully loaded for device specified by device_list[i];
 *  otherwise returns CL_INVALID_VALUE if lengths[i] is zero or if binaries[i] is a NULL value or CL_INVALID_BINARY in binary_status[i] if program binary is not a valid binary for the specified device.
 *  If binary_status is NULL , it is ignored.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateProgramWithBinary returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if the program object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if device_list is NULL or num_devices is zero.
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with context.
 *  - CL_INVALID_VALUE if lengths or binaries are NULL or if any entry in lengths[i] is zero or binaries[i] is NULL .
 *  - CL_INVALID_BINARY if an invalid program binary was encountered for any device. binary_status will return specific status for each device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  OpenCL allows applications to create a program object using the program source or binary and build appropriate program executables.
 *  This can be very useful as it allows applications to load program source and then compile and link to generate a program executable online on its first
 *  instance for appropriate OpenCL devices in the system. These executables can now be queried and cached by the application.
 *  Future instances of the application launching will no longer need to compile and link the program executables.
 *  The cached executables can be read and loaded by the application, which can help significantly reduce the application initialization time.
 *
 */
cl_program VC4CL_FUNC(clCreateProgramWithBinary)(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)

	if(num_devices == 0 || device_list == NULL)
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No devices specified!");

	if(num_devices > 1)
		//only 1 device supported
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Multiple devices specified, only a single is supported!");

	if(device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
		return returnError<cl_program>(CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Device specified is not the VC4CL GPU device!");

	if(lengths == NULL || binaries == NULL)
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No binary data given!");
	if(lengths[0] == 0 || binaries[0] == NULL)
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Empty binary data given!");

	//"The program binary can consist of either or both:
	// Device-specific code and/or,
	// Implementation-specific intermediate representation (IR) which will be converted to the device-specific code."
	// -> there is no implementation-specific IR, so only machine-code is supported

	//check whether the argument is a "valid" QPU code
	if(*((cl_uint*)binaries[0]) != VC4CL_BINARY_MAGIC_NUMBER)
		return returnError<cl_program>(CL_INVALID_BINARY, errcode_ret, __FILE__, __LINE__, "Invalid binary data given, magic number does not match!");

	const std::vector<char> buffer(binaries[0], binaries[0] + lengths[0]);
	Program* program = newObject<Program>(toType<Context>(context), buffer, true);
	CHECK_ALLOCATION_ERROR_CODE(program, errcode_ret, cl_program)

	if(binary_status != NULL)
		binary_status[0] = CL_SUCCESS;

	RETURN_OBJECT(program->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 136+:
 *
 *  Creates a program object for a context, and loads the information related to the built-in kernels into a program object.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param device_list is a pointer to a list of devices that are in context. device_list must be a non-NULL value.
 *  The built-in kernels are loaded for devices specified in this list. The devices associated with the program object will be the list of devices specified by device_list.
 *  The list of devices specified by device_list must be devices associated with context.
 *
 *  \param kernel_names is a semi-colon separated list of built-in kernel names.
 *
 *  \return clCreateProgramWithBuiltInKernels returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if the program object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if device_list is NULL or num_devices is zero.
 *  - CL_INVALID_VALUE if kernel_names is NULL or kernel_names contains a kernel name that is not supported by any of the devices in device_list.
 *  - CL_INVALID_DEVICE if devices listed in device_list are not in the list of devices associated with context.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_program VC4CL_FUNC(clCreateProgramWithBuiltInKernels)(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)

	if(num_devices == 0 || num_devices > 1 || device_list == NULL)
		//only 1 device supported
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Invalid number of devices given, a single is supported!");

	if(device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
		return returnError<cl_program>(CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Device specified is not the VC4CL GPU device!");

	//no built-in kernels are supported!

	return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "There are no supported built-in kernels");
}

/*!
 * OpenCL 1.2 specification, page 137:
 *
 *  Increments the program reference count. clCreateProgram does an implicit retain.
 *
 *  \return clRetainProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clRetainProgram)(cl_program program)
{
	CHECK_PROGRAM(toType<Program>(program));
	return toType<Program>(program)->retain();
}

/*!
 * OpenCL 1.2 specification, page 137:
 *
 *  Decrements the program reference count. The program object is deleted after all kernel objects associated with program have been deleted and the program reference count becomes zero.
 *
 *  \return clReleaseProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clReleaseProgram)(cl_program program)
{
	CHECK_PROGRAM(toType<Program>(program));
	return toType<Program>(program)->release();
}

/*!
 * OpenCL 1.2 specification, pages 138+:
 *
 *  Builds (compiles & links) a program executable from the program source or binary for all the devices or a specific device(s) in the OpenCL context associated with program.
 *  OpenCL allows program executables to be built using the source or the binary. clBuildProgram must be called for program created using either clCreateProgramWithSource or
 *  clCreateProgramWithBinary to build the program executable for one or more devices associated with program. If program is created with clCreateProgramWithBinary,
 *  then the program binary must be an executable binary (not a compiled binary or library).
 *  The executable binary can be queried using clGetProgramInfo(program, CL_PROGRAM_BINARIES , ...) and can be specified to clCreateProgramWithBinary to create a new program object.
 *
 *  \param program is the program object.
 *
 *  \param device_list is a pointer to a list of devices associated with program. If device_list is a NULL value,
 *  the program executable is built for all devices associated with program for which a source or binary has been loaded.
 *  If device_list is a non- NULL value, the program executable is built for devices specified in this list for which a source or binary has been loaded.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param options is a pointer to a null-terminated string of characters that describes the build options to be used for building the program executable.
 *  The list of supported options is described in section 5.6.4.
 *
 *  \param pfn_notify is a function pointer to a notification routine. The notification routine is a callback function that an application can register and which will be called when the program executable has been built (successfully or unsuccessfully).
 *  If pfn_notify is not NULL , clBuildProgram does not need to wait for the build to complete and can return immediately once the build operation can begin.
 *  The build operation can begin if the context, program whose sources are being compiled and linked,
 *  list of devices and build options specified are all valid and appropriate host and device resources needed to perform the build are available. If pfn_notify is NULL ,
 *  clBuildProgram does not return until the build has completed. This callback function may be called asynchronously by the OpenCL implementation.
 *  It is the application’s responsibility to ensure that the callback function is thread-safe.
 *
 *  \param user_data will be passed as an argument when pfn_notify is called. user_data can be NULL .
 *
 *  \return clBuildProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_INVALID_VALUE if device_list is NULL and num_devices is greater than zero, or if device_list is not NULL and num_devices is zero.
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL .
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with program
 *  - CL_INVALID_BINARY if program is created with clCreateProgramWithBinary and devices listed in device_list do not have a valid program binary loaded.
 *  - CL_INVALID_BUILD_OPTIONS if the build options specified by options are invalid.
 *  - CL_INVALID_OPERATION if the build of a program executable for any of the devices listed in device_list by a previous call to clBuildProgram for program has not completed.
 *  - CL_COMPILER_NOT_AVAILABLE if program is created with clCreateProgramWithSource and a compiler is not available i.e. CL_DEVICE_COMPILER_AVAILABLE specified in table 4.3 is set to CL_FALSE .
 *  - CL_BUILD_PROGRAM_FAILURE if there is a failure to build the program executable. This error will be returned if clBuildProgram does not return until the build has completed.
 *  - CL_INVALID_OPERATION if there are kernel objects attached to program.
 *  - CL_INVALID_OPERATION if program was not created with clCreateProgramWithSource or clCreateProgramWithBinary.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clBuildProgram)(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data)
{
	CHECK_PROGRAM(toType<Program>(program))
	cl_int state = CL_SUCCESS;
	if(toType<Program>(program)->getBuildStatus() != BuildStatus::COMPILED)
		//if the program was never build, compile. If it was already built once, re-compile
		state = VC4CL_FUNC(clCompileProgram)(program, num_devices, device_list, options, 0, NULL, NULL, pfn_notify, user_data);
	if(state != CL_SUCCESS)
	{
		return state;
	}

	VC4CL_FUNC(clLinkProgram)(toType<Program>(program)->context()->toBase(), num_devices, device_list, options, 1, &program, pfn_notify, user_data, &state);
	return state;
}

/*!
 * OpenCL 1.2 specification, pages 140+:
 *
 *  Compiles a program’s source for all the devices or a specific device(s) in the OpenCL context associated with program.
 *  The pre-processor runs before the program sources are compiled. The compiled binary is built for all devices associated with program or the list of devices specified.
 *  The compiled binary can be queried using clGetProgramInfo(program, CL_PROGRAM_BINARIES , ...) and can be specified to clCreateProgramWithBinary to create a new program object.
 *
 *  \param program is the program object that is the compilation target.
 *
 *  \param device_list is a pointer to a list of devices associated with program. If device_list is a NULL value, the compile is performed for all devices associated with program.
 *  If device_list is a non-NULL value, the compile is performed for devices specified in this list.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param options is a pointer to a null-terminated string of characters that describes the compilation options to be used for building the program executable.
 *  The list of supported options is as described in section 5.6.4.
 *
 *  \param num_input_headers specifies the number of programs that describe headers in the array referenced by input_headers.
 *
 *  \param input_headers is an array of program embedded headers created with clCreateProgramWithSource.
 *
 *  \param header_include_names is an array that has a one to one correspondence with input_headers.
 *  Each entry in header_include_names specifies the include name used by source in program that comes from an embedded header.
 *  The corresponding entry in input_headers identifies the program object which contains the header source to be used.
 *  The embedded headers are first searched before the headers in the list of directories specified by the –I compile option (as described in section 5.6.4.1).
 *  If multiple entries in header_include_names refer to the same header name, the first one encountered will be used.
 *
 *  \param pfn_notify is a function pointer to a notification routine. The notification routine is a callback function that an application can register and which will be called when the program executable
 *  has been built (successfully or unsuccessfully). If pfn_notify is not NULL , clCompileProgram does not need to wait for the compiler to complete and can return immediately once the
 *  compilation can begin. The compilation can begin if the context, program whose sources are being compiled, list of devices, input headers, programs that describe input headers and compiler
 *  options specified are all valid and appropriate host and device resources needed to perform the compile are available. If pfn_notify is NULL , clCompileProgram does not return until the
 *  compiler has completed. This callback function may be called asynchronously by the OpenCL implementation. It is the application’s responsibility to ensure that the callback function is
 *  thread-safe.
 *
 *  \param user_data will be passed as an argument when pfn_notify is called. user_data can be NULL .
 *
 *  \return clCompileProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_INVALID_VALUE if device_list is NULL and num_devices is greater than zero, or if device_list is not NULL and num_devices is zero.
 *  - CL_INVALID_VALUE if num_input_headers is zero and header_include_names or input_headers are not NULL or if num_input_headers is not zero and header_include_names or input_headers are NULL .
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL .
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with program
 *  - CL_INVALID_COMPILER_OPTIONS if the compiler options specified by options are invalid.
 *  - CL_INVALID_OPERATION if the compilation or build of a program executable for any of the devices listed in device_list by a previous call to clCompileProgram or clBuildProgram for program has not completed.
 *  - CL_COMPILER_NOT_AVAILABLE if a compiler is not available i.e. CL_DEVICE_COMPILER_AVAILABLE specified in table 4.3 is set to CL_FALSE .
 *  - CL_COMPILE_PROGRAM_FAILURE if there is a failure to compile the program source. This error will be returned if clCompileProgram does not return until the compile has completed.
 *  - CL_INVALID_OPERATION if there are kernel objects attached to program.
 *  - CL_INVALID_OPERATION if program has no source i.e. it has not been created with clCreateProgramWithSource.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clCompileProgram)(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data)
{
	CHECK_PROGRAM(toType<Program>(program))

	if(num_devices > 1 || (num_devices == 0 && device_list != NULL) || (num_devices > 0 && device_list == NULL))
		//only 1 device supported
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Only the single VC4CL GPU device is supported!");
	if(device_list != NULL && device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
		return returnError(CL_INVALID_DEVICE, __FILE__, __LINE__, "Invalid device given!");

	if((num_input_headers == 0 && (header_include_names != NULL || input_headers != NULL)) || (num_input_headers > 0 && (header_include_names == NULL || input_headers == NULL)))
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Invalid additional headers parameters!");

	if(pfn_notify == NULL && user_data != NULL)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "User data was set, but callback wasn't!");

	return toType<Program>(program)->compile( options == NULL ? "" : options, pfn_notify, user_data);
}

/*!
 * OpenCL 1.2 specification, pages 143+:
 *
 *  Links a set of compiled program objects and libraries for all the devices or a specific device(s) in the OpenCL context and creates an executable.
 *  clLinkProgram creates a new program object which contains this executable. The executable binary can be queried using clGetProgramInfo(program, CL_PROGRAM_BINARIES , ...)
 *  and can be specified to clCreateProgramWithBinary to create a new program object.
 *  The devices associated with the returned program object will be the list of devices specified by device_list or if device_list is NULL it will be the list of devices associated with context.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param device_list is a pointer to a list of devices that are in context. If device_list is a NULL value, the link is performed for all devices associated with context for which a compiled object is available.
 *  If device_list is a non- NULL value, the link is performed for devices specified in this list for which a compiled object is available.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param options is a pointer to a null-terminated string of characters that describes the link options to be used for building the program executable.
 *  The list of supported options is as described in section 5.6.5.
 *
 *  \param num_input_programs specifies the number of programs in array referenced by input_programs. input_programs is an array of program objects that are compiled binaries or libraries that are to
 *  be linked to create the program executable. For each device in device_list or if device_list is NULL the list of devices associated with context, the following cases occur:
 *   - All programs specified by input_programs contain a compiled binary or library for the device. In this case, a link is performed to generate a program executable for this device.
 *   - None of the programs contain a compiled binary or library for that device. In this case, no link is performed and there will be no program executable generated for this device.
 *   - All other cases will return a CL_INVALID_OPERATION error.
 *
 *  \param pfn_notify is a function pointer to a notification routine. The notification routine is a callback function that an application can register and which will be called when the program executable
 *  has been built (successfully or unsuccessfully). If pfn_notify is not NULL , clLinkProgram does not need to wait for the linker to complete and can return immediately once the linking operation can begin. Once the linker has completed,
 *  the pfn_notify callback function is called which returns the program object returned by clLinkProgram. The application can query the link status and log for this program object.
 *  This callback function may be called asynchronously by the OpenCL implementation. It is the application’s responsibility to ensure that the callback function is thread-safe.
 *  If pfn_notify is NULL , clLinkProgram does not return until the linker has completed.
 *
 *  \param user_data will be passed as an argument when pfn_notify is called. user_data can be NULL .
 *
 *  The linking operation can begin if the context, list of devices, input programs and linker options specified are all valid and appropriate host and device resources needed to perform the link are
 *  available. If the linking operation can begin, clLinkProgram returns a valid non-zero program object.
 *  If pfn_notify is NULL , the errcode_ret will be set to CL_SUCCESS if the link operation was successful and CL_LINK_FAILURE if there is a failure to link the compiled binaries and/or
 *  libraries. If pfn_notify is not NULL , clLinkProgram does not have to wait until the linker to complete and can return CL_SUCCESS in errcode_ret if the linking operation can begin.
 *  The pfn_notify callback function will return a CL_SUCCESS or CL_LINK_FAILURE if the linking operation was successful or not.
 *
 *  \return Otherwise clLinkProgram returns a NULL program object with an appropriate error in errcode_ret.
 *  The application should query the linker status of this program object to check if the link was successful or not. The list of errors that can be returned are:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if device_list is NULL and num_devices is greater than zero, or if device_list is not NULL and num_devices is zero.
 *  - CL_INVALID_VALUE if num_input_programs is zero and input_programs is NULL or if num_input_programs is zero and input_programs is not NULL or if num_input_programs is not zero and input_programs is NULL .
 *  - CL_INVALID_PROGRAM if programs specified in input_programs are not valid program objects.
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL .
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with context
 *  - CL_INVALID_LINKER_OPTIONS if the linker options specified by options are invalid.
 *  - CL_INVALID_OPERATION if the compilation or build of a program executable for any of the devices listed in device_list by a previous call to clCompileProgram or clBuildProgram for program has not completed.
 *  - CL_INVALID_OPERATION if the rules for devices containing compiled binaries or libraries as described in input_programs argument above are not followed.
 *  - CL_LINKER_NOT_AVAILABLE if a linker is not available i.e. CL_DEVICE_LINKER_AVAILABLE specified in table 4.3 is set to CL_FALSE .
 *  - CL_LINK_PROGRAM_FAILURE if there is a failure to link the compiled binaries and/or libraries.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_program VC4CL_FUNC(clLinkProgram)(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_programs, const cl_program* input_programs, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)
	if((num_devices == 0) != (device_list == NULL))
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No devices given!");
	if(num_devices > 1 || (device_list != NULL && device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase()))
		return returnError<cl_program>(CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Invalid device(s) given, only the VC4CL GPU device is supported!");
	if(num_input_programs == 0 || input_programs == NULL || num_input_programs > 1)
		return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Invalid input program");
	cl_program program = input_programs[0];
	CHECK_PROGRAM_ERROR_CODE(toType<Program>(program), errcode_ret, cl_program)
	cl_int status = toType<Program>(program)->link(options == nullptr ? "" : options, pfn_notify, user_data);

	if(status != CL_SUCCESS)
		return returnError<cl_program>(status, errcode_ret, __FILE__, __LINE__, "Linking failed!");

	RETURN_OBJECT(program, errcode_ret)
}

/*!
 * OpenCL 1.2 specification, page 150:
 *
 *  Allows the implementation to release the resources allocated by the OpenCL compiler for platform.
 *  This is a hint from the application and does not guarantee that the compiler will not be used in the future or that the compiler will actually be unloaded by the implementation.
 *  Calls to clBuildProgram, clCompileProgram or clLinkProgram after clUnloadPlatformCompiler will reload the compiler, if necessary, to build the appropriate program executable.
 *
 *  \return clUnloadPlatformCompiler returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_PLATFORM if platform is not a valid platform.
 */
cl_int VC4CL_FUNC(clUnloadPlatformCompiler)(cl_platform_id platform)
{
	//does nothing
	return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 151+:
 *
 *  Returns information about the program object.
 *
 *  \param program specifies the program object being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information returned in param_value by clGetProgramInfo is described in table 5.13.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be >= size of return type as described in table 5.13.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetProgramInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return type as described in table 5.13 and param_value is not NULL .
 *  - CL_INVALID_PROGRAM if program is a not a valid program object.
 *  - CL_INVALID_PROGRAM_EXECUTABLE if param_name is CL_PROGRAM_NUM_KERNELS or CL_PROGRAM_KERNEL_NAMES and a successful program executable has not been built for at least one device in the list of devices associated with program.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clUnloadCompiler)(void)
{
	//does nothing
	return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 154+:
 *
 *  Returns build information for each device in the program object.
 *
 *  \param program specifies the program object being queried.
 *
 *  \param device specifies the device for which build information is being queried. device must be a valid device associated with program.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information returned in param_value by clGetProgramBuildInfo is described in table 5.14.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be >= size of return type as described in table 5.14.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetProgramBuildInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_DEVICE if device is not in the list of devices associated with program.
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return type as described in table 5.14 and param_value is not NULL .
 *  - CL_INVALID_PROGRAM if program is a not a valid program object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clGetProgramInfo)(cl_program program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_PROGRAM(toType<Program>(program))
	return toType<Program>(program)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int VC4CL_FUNC(clGetProgramBuildInfo)(cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_PROGRAM(toType<Program>(program))
	CHECK_DEVICE(toType<Device>(device))
	return toType<Program>(program)->getBuildInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
