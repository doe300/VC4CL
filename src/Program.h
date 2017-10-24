/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_PROGRAM
#define VC4CL_PROGRAM

#include <vector>

#include "Object.h"
#include "Context.h"

namespace vc4cl
{
	enum class BuildStatus
	{
		//not yet built at all
		NOT_BUILD = 0,
		//compiled but not yet linked
		COMPILED = 1,
		//linked and compiled -> done
		DONE = 2
	};

	struct BuildInfo
	{
		cl_build_status status = CL_BUILD_NONE;
		std::string options;
		std::string log;
	};

	//THIS NEEDS TO HAVE THE SAME VALUES AS THE AddressSpace TYPE IN THE COMPILER!!
	enum class AddressSpace
	{
		GENERIC = 0,
		PRIVATE = 1,
		GLOBAL = 2,
		CONSTANT = 3,
		LOCAL = 4
	};

	struct ParamInfo
	{
		//the size of this parameter in bytes (e.g. 4 for pointers)
		cl_uchar size;
		//whether this parameter is a pointer to data
		cl_bool pointer;
		//whether this parameter is being written, only valid for pointers
		cl_bool output;
		//whether this parameter is being read, only valid for pointers
		cl_bool input;
		//whether this parameter is constant, only valid for pointers
		cl_bool constant;
		//whether the memory behind this parameter is guaranteed to not be aligned (overlap with other memory areas), only valid for pointers
		cl_bool restricted;
		//whether the memory behind this parameter is volatile, only valid for pointers
		cl_bool isVolatile;
		//the parameter name
		std::string name;
		//the parameter type-name, e.g. "<16 x i32>*"
		std::string type;
		//the number of components for vector-parameters
		cl_uchar elements;
		//the parameter's address space, only valid for pointers
		//OpenCL default address space is "private"
		AddressSpace addressSpace = AddressSpace::PRIVATE;
	};

	struct KernelInfo
	{
		//the offset of the instruction belonging to the kernel, in instructions (8 byte)
		cl_ushort offset;
		//the number of instructions in the kernel
		cl_ushort length;
		//the kernel-name
		std::string name;
		//the work-group size specified at compile-time
		std::array<std::size_t,kernel_config::NUM_DIMENSIONS> compileGroupSizes;
		//the info for all explicit parameters
		std::vector<ParamInfo> params;

		size_t getExplicitUniformCount() const;
	};

	typedef void(CL_CALLBACK* BuildCallback)(cl_program program, void* user_data);

	class Program: public Object<_cl_program, CL_INVALID_PROGRAM>, public HasContext
	{
	public:
		Program(Context* context, const std::vector<char>& code, const cl_bool isBinary);
		~Program();


		CHECK_RETURN cl_int compile(const std::string& options, BuildCallback callback, void* userData);
		CHECK_RETURN cl_int link(const std::string& options, BuildCallback callback, void* userData);
		CHECK_RETURN cl_int getInfo(cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
		CHECK_RETURN cl_int getBuildInfo(cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

		//the program's source, OpenCL C-code or LLVM IR / SPIR-V
		std::vector<char> sourceCode;
		//the machine-code, VC4C binary
		std::vector<char> binaryCode;
		//the global-data segment
		std::vector<char> globalData;

		BuildInfo buildInfo;

		//the kernel-infos, extracted from the VC4C binary
		//if this is set, the program is completely finished compiling
		std::vector<KernelInfo> kernelInfo;

		BuildStatus getBuildStatus() const;
	private:
		cl_int extractKernelInfo(cl_ulong** ptr, cl_uint* minKernelOffset);
	};

} /* namespace vc4cl */

#endif /* VC4CL_PROGRAM */
