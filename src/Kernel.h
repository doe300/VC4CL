/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_KERNEL
#define VC4CL_KERNEL

#include <vector>
#include <bitset>

#include "Object.h"
#include "Program.h"
#include "Event.h"


namespace vc4cl
{
	struct KernelArgument
	{
		union ScalarValue
		{
			float f;
			uint32_t u;
			int32_t s;
			void* ptr;
		};
		std::vector<ScalarValue> scalarValues;

		void addScalar(const float f);
		void addScalar(const uint32_t u);
		void addScalar(const int32_t s);
		void addScalar(void* ptr);

		std::string to_string() const;
	};

	class Kernel: public Object<_cl_kernel, CL_INVALID_KERNEL>
	{
	public:
		Kernel(Program* program, const KernelInfo& info);
		~Kernel();

		CHECK_RETURN cl_int setArg(cl_uint arg_index, size_t arg_size, const void* arg_value, const bool isSVMPointer = false);
		CHECK_RETURN cl_int getInfo(cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
		CHECK_RETURN cl_int getWorkGroupInfo(cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
		CHECK_RETURN cl_int getArgInfo(cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
		CHECK_RETURN cl_int enqueueNDRange(CommandQueue* commandQueue, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);

		object_wrapper<Program> program;
		KernelInfo info;

		std::vector<KernelArgument> args;
		std::bitset<VC4CL_MAX_PARAMETER> argsSetMask;
	};

	struct KernelExecution : public EventAction
	{
		object_wrapper<Kernel> kernel;
		cl_uchar numDimensions;
		size_t globalOffsets[VC4CL_NUM_DIMENSIONS];
		size_t globalSizes[VC4CL_NUM_DIMENSIONS];
		size_t localSizes[VC4CL_NUM_DIMENSIONS];

		KernelExecution(Kernel* kernel);
		virtual ~KernelExecution();

		cl_int operator()(Event* event) override;
	};

} /* namespace vc4cl */

#endif /* VC4CL_KERNEL */
