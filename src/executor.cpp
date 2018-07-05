/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Event.h"
#include "Kernel.h"
#include "Mailbox.h"
#include "V3D.h"

#include <CL/opencl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <thread>

using namespace vc4cl;

// timeout in ms
// to allow hanging kernels to time-out, set this to a non-infinite, but high enough value, so no valid kernel takes
// that long (e.g. 1min)
static const std::chrono::milliseconds KERNEL_TIMEOUT{30 * 1000};
// maximum number of work-groups to run in a single execution
// since all UNIFORMs (at least need to be re-loaded for every iteration, this number should not be too high
static const size_t MAX_ITERATIONS = 8;

// get_work_dim, get_local_size, get_local_id, get_num_groups (x, y, z), get_group_id (x, y, z), get_global_offset (x,
// y, z), global-data, repeat-iteration flag
static const unsigned MAX_HIDDEN_PARAMETERS = 14;

static unsigned AS_GPU_ADDRESS(const unsigned* ptr, DeviceBuffer* buffer)
{
    const char* tmp = *reinterpret_cast<const char**>(&ptr);
    return static_cast<unsigned>(
        static_cast<uint32_t>(buffer->qpuPointer) + ((tmp) - reinterpret_cast<char*>(buffer->hostPointer)));
}

static size_t get_size(size_t code_size, size_t num_uniforms, size_t global_data_size, size_t stackFrameSizeInWords)
{
    size_t raw_size = code_size + sizeof(unsigned) * num_uniforms + global_data_size +
        V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT) * stackFrameSizeInWords * sizeof(uint64_t) /* word-size */;
    // round up to next multiple of alignment
    return (raw_size / PAGE_ALIGNMENT + 1) * PAGE_ALIGNMENT;
}

static unsigned* set_work_item_info(unsigned* ptr, const cl_uint num_dimensions,
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& global_offsets,
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& global_sizes,
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& local_sizes,
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& group_indices,
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& local_indices, const unsigned global_data,
    const unsigned iterationIndex, const KernelUniforms& uniformsUsed)
{
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Setting work-item infos:" << std::endl;
    std::cout << "\t" << num_dimensions << " dimensions with offsets: " << global_offsets[0] << ", "
              << global_offsets[1] << ", " << global_offsets[2] << std::endl;
    std::cout << "\tGlobal IDs (sizes): " << (group_indices[0] + iterationIndex) * local_sizes[0] + local_indices[0]
              << "(" << global_sizes[0] << "), " << group_indices[1] * local_sizes[1] + local_indices[1] << "("
              << global_sizes[1] << "), " << group_indices[2] * local_sizes[2] + local_indices[2] << "("
              << global_sizes[2] << ")" << std::endl;
    std::cout << "\tLocal IDs (sizes): " << local_indices[0] << "(" << local_sizes[0] << "), " << local_indices[1]
              << "(" << local_sizes[1] << "), " << local_indices[2] << "(" << local_sizes[2] << ")" << std::endl;
    std::cout << "\tGroup IDs (sizes): " << (group_indices[0] + iterationIndex) << "("
              << (global_sizes[0] / local_sizes[0]) << "), " << group_indices[1] << "("
              << (global_sizes[1] / local_sizes[1]) << "), " << group_indices[2] << "("
              << (global_sizes[2] / local_sizes[2]) << ")" << std::endl;
#endif
    // composes UNIFORMS for the values
    if(uniformsUsed.getWorkDimensionsUsed())
        *ptr++ = num_dimensions; /* get_work_dim() */
    // since locals values top at 255, all 3 dimensions can be unified into one 32-bit UNIFORM
    // when read, the values are shifted by 8 * ndim bits and ANDed with 0xFF
    if(uniformsUsed.getLocalSizesUsed())
        *ptr++ = static_cast<unsigned>(
            local_sizes[2] << 16 | local_sizes[1] << 8 | local_sizes[0]); /* get_local_size(dim) */
    if(uniformsUsed.getLocalIDsUsed())
        *ptr++ = static_cast<unsigned>(
            local_indices[2] << 16 | local_indices[1] << 8 | local_indices[0]); /* get_local_id(dim) */
    if(uniformsUsed.getNumGroupsXUsed())
        *ptr++ = static_cast<unsigned>(global_sizes[0] / local_sizes[0]); /* get_num_groups(0) */
    if(uniformsUsed.getNumGroupsYUsed())
        *ptr++ = static_cast<unsigned>(global_sizes[1] / local_sizes[1]); /* get_num_groups(1) */
    if(uniformsUsed.getNumGroupsZUsed())
        *ptr++ = static_cast<unsigned>(global_sizes[2] / local_sizes[2]); /* get_num_groups(2) */
    if(uniformsUsed.getGroupIDXUsed())
        *ptr++ = static_cast<unsigned>(group_indices[0] + iterationIndex); /* get_group_id(0) */
    if(uniformsUsed.getGroupIDYUsed())
        *ptr++ = static_cast<unsigned>(group_indices[1]); /* get_group_id(1) */
    if(uniformsUsed.getGroupIDZUsed())
        *ptr++ = static_cast<unsigned>(group_indices[2]); /* get_group_id(2) */
    if(uniformsUsed.getGlobalOffsetXUsed())
        *ptr++ = static_cast<unsigned>(global_offsets[0]); /* get_global_offset(0) */
    if(uniformsUsed.getGlobalOffsetYUsed())
        *ptr++ = static_cast<unsigned>(global_offsets[1]); /* get_global_offset(1) */
    if(uniformsUsed.getGlobalOffsetZUsed())
        *ptr++ = static_cast<unsigned>(global_offsets[2]); /* get_global_offset(2) */
    if(uniformsUsed.getGlobalDataAddressUsed())
        *ptr++ = global_data; // base address for the global-data block
    return ptr;
}

static bool increment_index(std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& indices,
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS>& limits, const size_t offset)
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

static bool executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress, bool flushBuffer,
    std::chrono::milliseconds timeout)
{
#ifdef REGISTER_POKE_KERNELS
    return V3D::instance().executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
#else
    if(!flushBuffer)
        /*
         * For some reason, successive mailbox-calls without delays freeze the system (does the kernel get too
         * swamped??) A delay of 1ms has the same effect as no delay, 10ms slow down the execution, but work
         *
         * clpeak's global-bandwidth test runs ok without delay
         * clpeaks's compute-sp test hangs/freezes with/without delay
         */
        // TODO test with less delay? hangs? works? better performance?
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

    // the number of QPUs is the product of all local sizes
    const size_t num_qpus = args.localSizes[0] * args.localSizes[1] * args.localSizes[2];
    if(num_qpus > V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT))
        return CL_INVALID_GLOBAL_WORK_SIZE;

    // first work-group has group_ids 0,0,0
    const std::array<std::size_t, kernel_config::NUM_DIMENSIONS> group_limits = {
        args.globalSizes[0] / args.localSizes[0],
        args.globalSizes[1] / args.localSizes[1],
        args.globalSizes[2] / args.localSizes[2],
    };
    std::array<std::size_t, kernel_config::NUM_DIMENSIONS> group_indices = {0, 0, 0};
    std::array<std::size_t, kernel_config::NUM_DIMENSIONS> local_indices = {0, 0, 0};
    // Number of iterations for the "Kernel Loop Optimization"
    size_t numIterations = std::min(MAX_ITERATIONS, group_limits[0]);
    // make sure, the number of iterations divides the local size
    while(numIterations >= 1)
    {
        if(group_limits[0] % numIterations == 0)
            break;
        --numIterations;
    }

    /*
     * Allocate buffers for __local parameters
     *
     * The buffers are automatically cleaned up with leaving this function
     * and thus after the kernel has finished executing.
     */
    std::map<unsigned, std::unique_ptr<DeviceBuffer>> localBuffers;
    for(unsigned i = 0; i < kernel->args.size(); ++i)
    {
        const KernelArgument& arg = kernel->args.at(i);
        if(arg.sizeToAllocate > 0)
        {
            localBuffers.emplace(i, std::unique_ptr<DeviceBuffer>(mailbox().allocateBuffer(arg.sizeToAllocate)));
            if(kernel->program->context()->initializeMemoryToZero(CL_CONTEXT_MEMORY_INITIALIZE_LOCAL_KHR))
            {
                // we need to initialize the local memory to zero
                memset(localBuffers.at(i)->hostPointer, '\0', arg.sizeToAllocate);
            }
#ifdef DEBUG_MODE
            std::cout << "[VC4CL] Reserved " << arg.sizeToAllocate
                      << " bytes of buffer for local parameter: " << kernel->info.params.at(i).type << " "
                      << kernel->info.params.at(i).name << std::endl;
#endif
        }
    }

#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Running kernel '" << kernel->info.name << "' with " << kernel->info.getLength()
              << " instructions..." << std::endl;
    std::cout << "[VC4CL] Local sizes: " << args.localSizes[0] << " " << args.localSizes[1] << " " << args.localSizes[2]
              << " -> " << num_qpus << " QPUs" << std::endl;
    std::cout << "[VC4CL] Global sizes: " << args.globalSizes[0] << " " << args.globalSizes[1] << " "
              << args.globalSizes[2] << " -> "
              << (args.globalSizes[0] * args.globalSizes[1] * args.globalSizes[2]) / num_qpus << " work-groups ("
              << numIterations << " run at once)" << std::endl;
#endif

    //
    // ALLOCATE BUFFER
    //
    size_t buffer_size = get_size(kernel->info.getLength() * sizeof(uint64_t),
        num_qpus * numIterations * (MAX_HIDDEN_PARAMETERS + kernel->info.getExplicitUniformCount()),
        kernel->program->globalData.size() * sizeof(uint64_t), kernel->program->moduleInfo.getStackFrameSize());

    std::unique_ptr<DeviceBuffer> buffer(mailbox().allocateBuffer(static_cast<unsigned>(buffer_size)));
    if(!buffer)
        return CL_OUT_OF_RESOURCES;

    //
    // SET CONTENT
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
    unsigned* p = reinterpret_cast<unsigned*>(buffer->hostPointer);

    // Copy global data into GPU memory
    const unsigned global_data = AS_GPU_ADDRESS(p, buffer.get());
    void* data_start = kernel->program->globalData.data();
    const unsigned data_length = static_cast<unsigned>(kernel->program->globalData.size() * sizeof(uint64_t));
    memcpy(p, data_start, data_length);
    p += data_length / sizeof(unsigned);
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Copied " << data_length << " bytes of global data to device buffer" << std::endl;
#endif

    // Reserve space for stack-frames and fill it with zeros (e.g. for cl_khr_initialize_memory extension)
    uint32_t maxQPUS = V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT);
    uint32_t stackFrameSize = kernel->program->moduleInfo.getStackFrameSize() * sizeof(uint64_t);
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Reserving space for " << maxQPUS << " stack-frames of " << stackFrameSize << " bytes each"
              << std::endl;
#endif
    if(kernel->program->context()->initializeMemoryToZero(CL_CONTEXT_MEMORY_INITIALIZE_PRIVATE_KHR))
        memset(p, '\0', maxQPUS * stackFrameSize);
    p += (maxQPUS * stackFrameSize) / sizeof(unsigned);

    // Copy QPU program into GPU memory
    const unsigned* qpu_code = p;
    void* code_start = &kernel->program->binaryCode[kernel->info.getOffset()];
    memcpy(p, code_start, kernel->info.getLength() * sizeof(uint64_t));
    p += kernel->info.getLength() * sizeof(uint64_t) / sizeof(unsigned);
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Copied " << kernel->info.getLength() * sizeof(uint64_t)
              << " bytes of kernel code to device buffer" << std::endl;
#endif

    std::array<std::array<unsigned*, MAX_ITERATIONS>, 16> uniformPointers;
    // Build Uniforms
    const unsigned* qpu_uniform = p;
    for(unsigned i = 0; i < num_qpus; ++i)
    {
        for(int iteration = static_cast<int>(numIterations - 1); iteration >= 0; --iteration)
        {
            uniformPointers.at(i).at(iteration) = p;
            p = set_work_item_info(p, args.numDimensions, args.globalOffsets, args.globalSizes, args.localSizes,
                group_indices, local_indices, global_data, static_cast<unsigned>(numIterations - 1) - iteration,
                kernel->info.uniformsUsed);
            for(unsigned u = 0; u < kernel->info.params.size(); ++u)
            {
                KernelArgument& arg = kernel->args.at(u);
                if(localBuffers.find(u) != localBuffers.end())
                {
                    // there exists a temporary buffer for the __local parameter, so set its address as kernel argument
                    arg.scalarValues.clear();
                    arg.addScalar(localBuffers.at(u)->qpuPointer);
                }
#ifdef DEBUG_MODE
                std::cout << "[VC4CL] Setting parameter " << (kernel->info.uniformsUsed.countUniforms() + u) << " to "
                          << arg.to_string() << std::endl;
#endif
                for(cl_uchar i = 0; i < kernel->info.params[u].getElements(); ++i)
                    *p++ = arg.scalarValues.at(i).getUnsigned();
            }
            //"Kernel Loop Optimization" to repeat kernel for several work-groups
            // needs to be non-zero for all but the last iteration and zero for the last iteration
            *p++ = static_cast<unsigned>(iteration);
        }
#ifdef DEBUG_MODE
        std::cout << "[VC4CL] "
                  << numIterations *
                (kernel->info.uniformsUsed.countUniforms() + 1 /* re-run flag */ + kernel->info.params.size())
                  << " parameters set." << std::endl;
#endif
        increment_index(local_indices, args.localSizes, 1);
    }

    /* Build QPU Launch messages */
    unsigned* qpu_msg = p;
    for(unsigned i = 0; i < num_qpus; ++i)
    {
        *p++ = AS_GPU_ADDRESS(qpu_uniform +
                i * numIterations *
                    (kernel->info.uniformsUsed.countUniforms() + 1 /* re-run flag */ +
                        kernel->info.getExplicitUniformCount()),
            buffer.get());
        *p++ = AS_GPU_ADDRESS(qpu_code, buffer.get());
    }

#ifdef DEBUG_MODE
    {
        static const std::string dumpFile("/tmp/vc4cl-dump.bin");
        std::cout << "[VC4CL] Dumping kernel buffer to " << dumpFile << std::endl;
        std::ofstream f(dumpFile, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        // add additional pointers for the dump-analyzer
        // qpu base-pointer (global-data pointer) | qpu code-pointer | qpu UNIFORM-pointer | num uniforms per iteration
        // | num iterations | implicit uniform bit-field
        unsigned tmp = static_cast<unsigned>(buffer->qpuPointer);
        f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
        tmp = AS_GPU_ADDRESS(qpu_code, buffer.get());
        f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
        tmp = AS_GPU_ADDRESS(qpu_uniform, buffer.get());
        f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
        uint16_t tmp16 = static_cast<uint16_t>(
            kernel->info.uniformsUsed.countUniforms() + 1 /* re-run flag */ + kernel->info.getExplicitUniformCount());
        f.write(reinterpret_cast<char*>(&tmp16), sizeof(uint16_t));
        tmp16 = static_cast<uint16_t>(numIterations);
        f.write(reinterpret_cast<char*>(&tmp16), sizeof(uint16_t));
        tmp = static_cast<unsigned>(kernel->info.uniformsUsed.value);
        f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
        // write buffer contents
        f.write(reinterpret_cast<char*>(buffer->hostPointer), buffer_size);
        f.close();
    }
#endif

        //
        // EXECUTION
        //
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Running work-group " << group_indices[0] << ", " << group_indices[1] << ", "
              << group_indices[2] << std::endl;
#endif
    // on first execution, flush code cache
    bool result = executeQPU(static_cast<unsigned>(num_qpus),
        std::make_pair(qpu_msg, AS_GPU_ADDRESS(qpu_msg, buffer.get())), true, KERNEL_TIMEOUT);
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Execution: " << (result ? "successful" : "failed") << std::endl;
#endif
    if(!result)
        return CL_OUT_OF_RESOURCES;
    while(increment_index(group_indices, group_limits, numIterations))
    {
        local_indices[0] = local_indices[1] = local_indices[2] = 0;
        // re-set indices and offsets for all QPUs
        for(cl_uint i = 0; i < num_qpus; ++i)
        {
            for(int iteration = static_cast<int>(numIterations - 1); iteration >= 0; --iteration)
            {
                set_work_item_info(uniformPointers.at(i).at(iteration), args.numDimensions, args.globalOffsets,
                    args.globalSizes, args.localSizes, group_indices, local_indices, global_data,
                    static_cast<unsigned>(numIterations - 1) - iteration, kernel->info.uniformsUsed);
            }
            increment_index(local_indices, args.localSizes, 1);
        }
#ifdef DEBUG_MODE
        std::cout << "[VC4CL] Running work-group " << group_indices[0] << ", " << group_indices[1] << ", "
                  << group_indices[2] << std::endl;
#endif
        // all following executions, don't flush cache
        result = executeQPU(static_cast<unsigned>(num_qpus),
            std::make_pair(qpu_msg, AS_GPU_ADDRESS(qpu_msg, buffer.get())), false, KERNEL_TIMEOUT);
#ifdef DEBUG_MODE
        std::cout << "[VC4CL] Execution: " << (result ? "successful" : "failed") << std::endl;
#endif
        if(!result)
            return CL_OUT_OF_RESOURCES;
    }

    //
    // CLEANUP
    //

    if(result)
        return CL_COMPLETE;
    return CL_OUT_OF_RESOURCES;
}
