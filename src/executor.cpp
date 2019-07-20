/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Buffer.h"
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
    LOG(std::cout << "Setting work-item infos:" << std::endl;
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
                  << (global_sizes[2] / local_sizes[2]) << ")" << std::endl)
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

cl_int executeKernel(KernelExecution& args)
{
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

#ifdef DEBUG_MODE
    LOG(std::cout << "Running kernel '" << kernel->info.name << "' with " << kernel->info.getLength()
                  << " instructions..." << std::endl)
    LOG(std::cout << "Local sizes: " << args.localSizes[0] << " " << args.localSizes[1] << " " << args.localSizes[2]
                  << " -> " << num_qpus << " QPUs" << std::endl)
    LOG(std::cout << "Global sizes: " << args.globalSizes[0] << " " << args.globalSizes[1] << " " << args.globalSizes[2]
                  << " -> " << (args.globalSizes[0] * args.globalSizes[1] * args.globalSizes[2]) / num_qpus
                  << " work-groups (" << numIterations << " run at once)" << std::endl)
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
    LOG(std::cout << "Copied " << data_length << " bytes of global data to device buffer" << std::endl)
#endif

    // Reserve space for stack-frames and fill it with zeros (e.g. for cl_khr_initialize_memory extension)
    uint32_t maxQPUS = V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT);
    uint32_t stackFrameSize = static_cast<uint32_t>(kernel->program->moduleInfo.getStackFrameSize() * sizeof(uint64_t));
#ifdef DEBUG_MODE
    LOG(std::cout << "Reserving space for " << maxQPUS << " stack-frames of " << stackFrameSize << " bytes each"
                  << std::endl)
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
    LOG(std::cout << "Copied " << kernel->info.getLength() * sizeof(uint64_t)
                  << " bytes of kernel code to device buffer" << std::endl)
#endif

    std::array<std::array<unsigned*, MAX_ITERATIONS>, 16> uniformPointers;
    // Build Uniforms
    const unsigned* qpu_uniform = p;
    for(unsigned i = 0; i < num_qpus; ++i)
    {
        for(int iteration = static_cast<int>(numIterations - 1); iteration >= 0; --iteration)
        {
            uniformPointers.at(i).at(static_cast<unsigned>(iteration)) = p;
            p = set_work_item_info(p, args.numDimensions, args.globalOffsets, args.globalSizes, args.localSizes,
                group_indices, local_indices, global_data,
                static_cast<unsigned>(numIterations - 1) - static_cast<unsigned>(iteration), kernel->info.uniformsUsed);
            for(unsigned u = 0; u < kernel->info.params.size(); ++u)
            {
                auto tmpBufferIt = args.tmpBuffers.find(u);
                auto persistentBufferIt = args.persistentBuffers.find(u);
                if(tmpBufferIt != args.tmpBuffers.end())
                {
                    // there exists a temporary buffer for the __local/struct parameter, so set its address as kernel
                    // argument
                    *p++ = static_cast<unsigned>(tmpBufferIt->second->qpuPointer);
#ifdef DEBUG_MODE
                    LOG(std::cout << "Setting parameter " << (kernel->info.uniformsUsed.countUniforms() + u)
                                  << " to temporary buffer 0x" << std::hex << tmpBufferIt->second->qpuPointer
                                  << std::dec << std::endl)
#endif
                }
                else if(persistentBufferIt != args.persistentBuffers.end())
                {
                    // the argument is a pointer to a buffer, use its device pointer as kernel argument value.
                    // Since the buffer pointer might be NULL, we have to check for this first
                    auto devicePtr = persistentBufferIt->second.get() ?
                        static_cast<unsigned>(persistentBufferIt->second->qpuPointer) :
                        0;
                    *p++ = devicePtr;
#ifdef DEBUG_MODE
                    LOG(std::cout << "Setting parameter " << (kernel->info.uniformsUsed.countUniforms() + u)
                                  << " to buffer 0x" << std::hex << devicePtr << std::dec << std::endl)
#endif
                }
                else if(auto scalarArg = dynamic_cast<const ScalarArgument*>(kernel->args.at(u).get()))
                {
                    // "default" scalar or vector of scalar kernel argument
                    for(cl_uchar i = 0; i < kernel->info.params[u].getVectorElements(); ++i)
                        *p++ = scalarArg->scalarValues.at(i).getUnsigned();
#ifdef DEBUG_MODE
                    LOG(std::cout << "Setting parameter " << (kernel->info.uniformsUsed.countUniforms() + u)
                                  << " to scalar " << scalarArg->to_string() << std::endl)
#endif
                }
                else
                {
                    // At this point all argument types should be handled already
                    return CL_INVALID_KERNEL_ARGS;
                }
            }
            //"Kernel Loop Optimization" to repeat kernel for several work-groups
            // needs to be non-zero for all but the last iteration and zero for the last iteration
            *p++ = static_cast<unsigned>(iteration);
        }
#ifdef DEBUG_MODE
        LOG(std::cout << numIterations *
                (kernel->info.uniformsUsed.countUniforms() + 1 /* re-run flag */ + kernel->info.params.size())
                      << " parameters set." << std::endl)
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

#if 0
    const std::string dumpFile("/tmp/vc4cl-dump-" + kernel->info.name + "-" + std::to_string(rand()) + ".bin");
    std::map<unsigned, const DeviceBuffer*> bufferArguments;
    {
        LOG(std::cout << "Dumping kernel buffer to " << dumpFile << std::endl)
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
        f.write(reinterpret_cast<char*>(buffer->hostPointer), static_cast<uint32_t>(buffer_size));
        // append additionally the kernel parameter for this execution
        // append 0-word as border between sections
        tmp = 0;
        f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
        // parameter have this format: pointer-bit| # words | <data (direct or buffer contents)>
        for(unsigned i = 0; i < kernel->args.size(); ++i)
        {
            const auto& info = kernel->info.params[i];
            const auto& arg = kernel->args[i];
            if(info.getPointer() && arg.scalarValues.at(0).getUnsigned() == global_data)
            {
                tmp = 0x80000000 | static_cast<uint32_t>(data_length / sizeof(unsigned));
                f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                f.write(reinterpret_cast<const char*>(buffer->hostPointer), data_length);
            }
            else if(info.getPointer())
            {
                // search for local/struct buffers allocated by the kernel execution
                const DeviceBuffer* buffer = nullptr;
                if(tmpBuffers.find(i) != tmpBuffers.end())
                    buffer = tmpBuffers[i].get();
                else
                {
                    // fall back to global memory objects
                    auto tmpBuffer = static_cast<const Buffer*>(
                        ObjectTracker::findTrackedObject([&](const BaseObject& base) -> bool {
                            if(base.typeName == _cl_mem::TYPE_NAME || strcmp(base.typeName, _cl_mem::TYPE_NAME) == 0)
                            {
                                const auto& buffer = static_cast<const vc4cl::Buffer&>(base);
                                const auto& devBuffer = buffer.deviceBuffer;
                                return devBuffer &&
                                    static_cast<uint32_t>(devBuffer->qpuPointer) ==
                                    arg.scalarValues.at(0).getUnsigned();
                            }
                            return false;
                        }));
                    if(tmpBuffer)
                        buffer = tmpBuffer->deviceBuffer.get();
                }
                if(buffer == nullptr)
                {
                    tmp = 1;
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                    tmp = arg.scalarValues.front().getUnsigned();
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                }
                else
                {
                    bufferArguments.emplace(i, buffer);
                    tmp = 0x80000000 | static_cast<uint32_t>(buffer->size / sizeof(unsigned));
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                    f.write(reinterpret_cast<const char*>(buffer->hostPointer), buffer->size);
                }
            }
            else
            {
                tmp = static_cast<uint32_t>(arg.scalarValues.size());
                f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                for(const auto elem : arg.scalarValues)
                {
                    tmp = elem.getUnsigned();
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                }
            }
        }
    }
#endif

        //
        // EXECUTION
        //
#ifdef DEBUG_MODE
    LOG(std::cout << "Running work-group " << group_indices[0] << ", " << group_indices[1] << ", " << group_indices[2]
                  << std::endl)
#endif
    // on first execution, flush code cache
    bool result = executeQPU(static_cast<unsigned>(num_qpus),
        std::make_pair(qpu_msg, AS_GPU_ADDRESS(qpu_msg, buffer.get())), true, KERNEL_TIMEOUT);
#ifdef DEBUG_MODE
    LOG(std::cout << "Execution: " << (result ? "successful" : "failed") << std::endl)
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
                set_work_item_info(uniformPointers.at(i).at(static_cast<unsigned>(iteration)), args.numDimensions,
                    args.globalOffsets, args.globalSizes, args.localSizes, group_indices, local_indices, global_data,
                    static_cast<unsigned>(numIterations - 1) - static_cast<unsigned>(iteration),
                    kernel->info.uniformsUsed);
            }
            increment_index(local_indices, args.localSizes, 1);
        }
#ifdef DEBUG_MODE
        LOG(std::cout << "Running work-group " << group_indices[0] << ", " << group_indices[1] << ", "
                      << group_indices[2] << std::endl)
#endif
        // all following executions, don't flush cache
        result = executeQPU(static_cast<unsigned>(num_qpus),
            std::make_pair(qpu_msg, AS_GPU_ADDRESS(qpu_msg, buffer.get())), false, KERNEL_TIMEOUT);
#ifdef DEBUG_MODE
        LOG(std::cout << "Execution: " << (result ? "successful" : "failed") << std::endl)
#endif
        if(!result)
            return CL_OUT_OF_RESOURCES;
    }

#if 0
    {
        std::ofstream f(dumpFile, std::ios_base::out | std::ios_base::app | std::ios_base::binary);
        // append additionally the kernel parameter for this execution
        // append all-bits-set-word as border between sections
        unsigned tmp = 0xFFFFFFFF;
        f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
        // parameter have this format: pointer-bit| # words | <data (direct or buffer contents)>
        for(unsigned i = 0; i < kernel->args.size(); ++i)
        {
            const auto& info = kernel->info.params[i];
            const auto& arg = kernel->args[i];
            if(info.getPointer() && arg.scalarValues.at(0).getUnsigned() == global_data)
            {
                tmp = 0x80000000 | static_cast<uint32_t>(data_length / sizeof(unsigned));
                f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                f.write(reinterpret_cast<const char*>(buffer->hostPointer), data_length);
            }
            else if(info.getPointer())
            {
                auto bufferIt = bufferArguments.find(i);
                if(bufferIt == bufferArguments.end())
                {
                    tmp = 1;
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                    tmp = arg.scalarValues.front().getUnsigned();
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                }
                else
                {
                    //FIXME SEGFAULTS if buffer already freed! -> error in client
                    tmp = 0x80000000 | static_cast<uint32_t>(bufferIt->second->size / sizeof(unsigned));
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                    f.write(reinterpret_cast<const char*>(bufferIt->second->hostPointer), bufferIt->second->size);
                }
            }
            else
            {
                tmp = static_cast<uint32_t>(arg.scalarValues.size());
                f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                for(const auto elem : arg.scalarValues)
                {
                    tmp = elem.getUnsigned();
                    f.write(reinterpret_cast<char*>(&tmp), sizeof(unsigned));
                }
            }
        }
    }
#endif

    //
    // CLEANUP
    //

    // even though the buffers are already freed when the KernelExecution event is freed, we clear the maps here, since
    // we do not need the buffers anymore
    args.tmpBuffers.clear();
    args.persistentBuffers.clear();

    if(result)
        return CL_COMPLETE;
    return CL_OUT_OF_RESOURCES;
}
