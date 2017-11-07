/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <CL/opencl.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <thread>
#include <chrono>

#include "common.h"
#include "Event.h"
#include "Kernel.h"
#include "Mailbox.h"
#include "V3D.h"

using namespace vc4cl;

//timeout in ms
//to allow hanging kernels to time-out, set this to a non-infinite, but high enough value, so no valid kernel takes that long (e.g. 1min)
static const std::chrono::milliseconds KERNEL_TIMEOUT{60 * 1000};
//maximum number of work-groups to run in a single execution
//since all UNIFORMs (at least need to be re-loaded for every iteration, this number should not be too high
static const size_t MAX_ITERATIONS = 8;

//get_work_dim, get_local_size, get_local_id, get_num_groups (x, y, z), get_group_id (x, y, z), get_global_offset (x, y, z), global-data, repeat-iteration flag
static const unsigned NUM_HIDDEN_PARAMETERS = 14;

//#define AS_GPU_ADDRESS(x) (uintptr_t) buffer.qpuPointer + ((reinterpret_cast<const void*>(x)) - buffer.hostPointer)
static uintptr_t AS_GPU_ADDRESS(const unsigned* ptr, DeviceBuffer* buffer)
{
	const char* tmp = *reinterpret_cast<const char**>(&ptr);
	return (uintptr_t) buffer->qpuPointer + ((tmp) - (char*)buffer->hostPointer);
}

static size_t get_size(size_t code_size, size_t num_uniforms, size_t global_data_size)
{
	size_t raw_size = code_size + sizeof(unsigned) * num_uniforms + global_data_size;
	//round up to next multiple of alignment
	return (raw_size / PAGE_ALIGNMENT + 1) * PAGE_ALIGNMENT;
}

static unsigned* set_work_item_info(unsigned* ptr, const cl_uint num_dimensions, const std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& global_offsets, const std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& global_sizes, const std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& local_sizes, const std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& group_indices, const std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& local_indices, const void* global_data, const unsigned iterationIndex)
{
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Setting work-item infos:" << std::endl;
	std::cout << "\t" << num_dimensions << " dimensions with offsets: " << global_offsets[0] << ", " << global_offsets[1] << ", " << global_offsets[2] << std::endl;
	std::cout << "\tGlobal IDs (sizes): " << (group_indices[0] + iterationIndex) * local_sizes[0] + local_indices[0] << "(" << global_sizes[0] << "), "
			<< group_indices[1] * local_sizes[1] + local_indices[1] << "(" << global_sizes[1] << "), "
			<< group_indices[2] * local_sizes[2] + local_indices[2] << "(" << global_sizes[2] << ")" << std::endl;
	std::cout << "\tLocal IDs (sizes): " << local_indices[0] << "(" << local_sizes[0] << "), "
			<< local_indices[1] << "(" << local_sizes[1] << "), " << local_indices[2] << "(" << local_sizes[2] << ")" << std::endl;
	std::cout << "\tNumber of groups: " << (global_sizes[0] / local_sizes[0]) << ", " << (global_sizes[1] / local_sizes[1]) << ", " << (global_sizes[2] / local_sizes[2]) << std::endl;
#endif
	//composes UNIFORMS for the values
	*ptr++ = num_dimensions;	/* get_work_dim() */
	//since locals values top at 255, all 3 dimensions can be unified into one 32-bit UNIFORM
	//when read, the values are shifted by 8 * ndim bits and ANDed with 0xFF
	*ptr++ = local_sizes[2] << 16 | local_sizes[1] << 8 | local_sizes[0];	/* get_local_size(dim) */
	*ptr++ = local_indices[2] << 16 | local_indices[1] << 8 | local_indices[0];	/* get_local_id(dim) */
	*ptr++ = global_sizes[0] / local_sizes[0];	/* get_num_groups(0) */
	*ptr++ = global_sizes[1] / local_sizes[1];	/* get_num_groups(1) */
	*ptr++ = global_sizes[2] / local_sizes[2];	/* get_num_groups(2) */
	*ptr++ = group_indices[0] + iterationIndex; /* get_group_id(0) */
	*ptr++ = group_indices[1]; /* get_group_id(1) */
	*ptr++ = group_indices[2]; /* get_group_id(2) */
	*ptr++ = global_offsets[0];	/* get_global_offset(0) */
	*ptr++ = global_offsets[1];	/* get_global_offset(1) */
	*ptr++ = global_offsets[2];	/* get_global_offset(2) */
	*ptr++ = reinterpret_cast<std::uintptr_t>(global_data);	//base address for the global-data block
	return ptr;
}

static cl_bool increment_index(std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& indices, const std::array<std::size_t,kernel_config::NUM_DIMENSIONS>& limits, const size_t offset)
{
	indices[0] += offset;
	if(indices[0] >= limits[0])
	{
		indices[0] -= limits[0];
		++indices[1];
		if(indices[1] == limits[1])
		{
			indices[1] = 0;
			++indices[2];
		}
	}
	return indices[2] < limits[2];
}

static bool executeQPU(unsigned numQPUs, std::pair<uint32_t*, uintptr_t> controlAddress, bool flushBuffer, std::chrono::milliseconds timeout)
{
#ifdef REGISTER_POKE_KERNELS
	return V3D::instance().executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
#else
	if(!flushBuffer)
		//for some reason, successive mailbox-calls without delays freeze the system (does the kernel get too swamped??)
		//TODO test with less delay? hangs? works? better performance?
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return mailbox().executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
#endif
}

cl_int executeKernel(Event* event)
{
	CHECK_EVENT(event)
	KernelExecution& args = dynamic_cast<KernelExecution&>(*event->action.get());
	Kernel* kernel = args.kernel.get();
	CHECK_KERNEL(kernel)
	
	//the number of QPUs is the product of all local sizes
	const size_t num_qpus = args.localSizes[0] * args.localSizes[1] * args.localSizes[2];
	if(num_qpus > V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT))
		return CL_INVALID_GLOBAL_WORK_SIZE;
	

	//first work-group has group_ids 0,0,0
	const std::array<std::size_t,kernel_config::NUM_DIMENSIONS> group_limits = {
		args.globalSizes[0] / args.localSizes[0],
		args.globalSizes[1] / args.localSizes[1],
		args.globalSizes[2] / args.localSizes[2],
	};
	std::array<std::size_t,kernel_config::NUM_DIMENSIONS> group_indices = {0, 0, 0};
	std::array<std::size_t,kernel_config::NUM_DIMENSIONS> local_indices = {0, 0, 0};
	//Number of iterations for the "Kernel Loop Optimization"
	size_t numIterations = std::min(MAX_ITERATIONS, group_limits[0]);
	//make sure, the number of iterations divides the local size
	while(numIterations >= 1)
	{
		if(group_limits[0] % numIterations == 0)
			break;
		--numIterations;
	}

#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Running kernel '" << kernel->info.name << "' with " << kernel->info.length << " instructions..." << std::endl;
	std::cout << "[VC4CL] Local sizes: " << args.localSizes[0] << " " << args.localSizes[1] << " " << args.localSizes[2] << " -> " << num_qpus << " QPUs\n" << std::endl;
	std::cout << "[VC4CL] Global sizes: " << args.globalSizes[0] << " " << args.globalSizes[1] << " " << args.globalSizes[2] << " -> " << (args.globalSizes[0] * args.globalSizes[1] * args.globalSizes[2]) / num_qpus << " work-groups (" << numIterations << " run at once)" << std::endl;
#endif
	
	//
	// ALLOCATE BUFFER
	//
	size_t buffer_size = get_size(kernel->info.getLength() * 8, num_qpus * numIterations * (NUM_HIDDEN_PARAMETERS + kernel->info.getExplicitUniformCount()), kernel->program->globalData.size());

	std::unique_ptr<DeviceBuffer> buffer(mailbox().allocateBuffer(buffer_size));
	if(buffer.get() == nullptr)
		return CL_OUT_OF_RESOURCES;
	
	//
	//SET CONTENT
	//
	/*
	 * source: https://github.com/hermanhermitage/videocoreiv-qpu/blob/master/qpu-tutorial/qpu-02.c
	 * 
	 * +---------------+
	 * |  Data Segment |
	 * +---------------+ <----+
	 * |  QPU Code     |      |
	 * |  ...          |      |
	 * +---------------+ <--+ |
	 * |  Uniforms     |    | |
	 * +---------------+    | |
	 * |  QPU0 Uniform -----+ |
	 * |  QPU0 Start   -------+
	 * +---------------+
	 */
	unsigned* p = reinterpret_cast<unsigned *>(buffer->hostPointer);
	
	//Copy global data into GPU memory
	const void* global_data = reinterpret_cast<void*>(AS_GPU_ADDRESS(p, buffer.get()));
	void* data_start = kernel->program->globalData.data();
	const unsigned data_length = kernel->program->globalData.size();
	memcpy(p, data_start, data_length);
	p += data_length / sizeof(unsigned);
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Copied " << data_length << " bytes of global data to device buffer" << std::endl;
#endif

	// Copy QPU program into GPU memory
	const unsigned* qpu_code = p;
	void* code_start = kernel->program->binaryCode.data() + (kernel->info.getOffset() * 8);
	memcpy(p, code_start, kernel->info.getLength() * 8);
	p += kernel->info.getLength() * 8 / sizeof(unsigned);
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Copied " << kernel->info.length * sizeof(int64_t) << " bytes of kernel code to device buffer" << std::endl;
#endif

	unsigned* uniform_pointers[16][MAX_ITERATIONS];
	// Build Uniforms
	const unsigned* qpu_uniform = p;
	for (unsigned i = 0; i < num_qpus; ++i) {
		for(int iteration = numIterations - 1; iteration >= 0; --iteration)
		{
			uniform_pointers[i][iteration] = p;
			p = set_work_item_info(p, args.numDimensions, args.globalOffsets, args.globalSizes, args.localSizes, group_indices, local_indices, global_data, (numIterations - 1) - iteration);
			for(unsigned u = 0; u < kernel->info.params.size(); ++u)
			{
#ifdef DEBUG_MODE
				std::cout << "[VC4CL] Setting parameter " << (NUM_HIDDEN_PARAMETERS - 1) + u << " to " << kernel->args[u].to_string() << std::endl;
#endif
				for(cl_uchar i = 0; i < kernel->info.params[u].getElements(); ++i)
					*p++ = kernel->args[u].scalarValues.at(i).getUnsigned();
			}
			//"Kernel Loop Optimization" to repeat kernel for several work-groups
			//needs to be non-zero for all but the last iteration and zero for the last iteration
			*p++ = static_cast<unsigned>(iteration);
		}
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] " << numIterations * (NUM_HIDDEN_PARAMETERS + kernel->info.params.size()) << " parameters set." << std::endl;
#endif
		increment_index(local_indices, args.localSizes, 1);
	}
	
	/* Build QPU Launch messages */
	unsigned *qpu_msg = p;
	for (unsigned i = 0; i < num_qpus; ++i) {
		*p++ = AS_GPU_ADDRESS(qpu_uniform + i * numIterations * (NUM_HIDDEN_PARAMETERS + kernel->info.getExplicitUniformCount()), buffer.get());
		*p++ = AS_GPU_ADDRESS(qpu_code, buffer.get());
	}
	
	//
	// EXECUTION
	//
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Running work-group "<< group_indices[0] << ", " << group_indices[1] << ", " << group_indices[2] << std::endl;
#endif
	//on first execution, flush code cache
	bool result = executeQPU(num_qpus, std::make_pair(qpu_msg, AS_GPU_ADDRESS(qpu_msg, buffer.get())), true, KERNEL_TIMEOUT);
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Execution: " << (result == true ? "successful" : "failed") << std::endl;
#endif
	if(result == false)
		return CL_OUT_OF_RESOURCES;
	while(increment_index(group_indices, group_limits, numIterations))
	{
		local_indices[0] = local_indices[1] = local_indices[2] = 0;
		//re-set indices and offsets for all QPUs
		for(cl_uint i = 0; i < num_qpus; ++i)
		{
			for(int iteration = numIterations - 1; iteration >= 0; --iteration)
			{
				set_work_item_info(uniform_pointers[i][iteration], args.numDimensions, args.globalOffsets, args.globalSizes, args.localSizes, group_indices, local_indices, global_data, (numIterations - 1) - iteration);
			}
			increment_index(local_indices, args.localSizes, 1);
		}
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Running work-group " << group_indices[0] << ", " << group_indices[1] << ", " << group_indices[2] << std::endl;
#endif
		//all following executions, don't flush cache
		//TODO test effect of turning on/off cache flush
		result = executeQPU(num_qpus, std::make_pair(qpu_msg, AS_GPU_ADDRESS(qpu_msg, buffer.get())), false, KERNEL_TIMEOUT);
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Execution: " << (result == true ? "successful" : "failed") << std::endl;
#endif
		if(result == false)
			return CL_OUT_OF_RESOURCES;
	}
	
	//
	// CLEANUP
	//

	if(result)
		return CL_COMPLETE;
	return CL_OUT_OF_RESOURCES;
}
