/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "VCHI.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

using namespace vc4cl;

// This is the only usage I could find for executing code:
// https://searchcode.com/total-file/116503467/

#ifdef __cplusplus
extern "C"
{
#endif

    // see "/opt/vc/include/interface/vchi/vchi.h"
    struct opaque_vchi_instance_handle_t;
    struct vchi_connection_t;

    int32_t vchi_initialise(opaque_vchi_instance_handle_t** instance_handle);
    int32_t vchi_connect(vchi_connection_t** connections, const uint32_t num_connections,
        opaque_vchi_instance_handle_t* instance_handle);
    int32_t vchi_disconnect(opaque_vchi_instance_handle_t* instance_handle);

    // see "/opt/vc/include/interface/vmcs_host/vc_vchi_gencmd.h"
    void vc_vchi_gencmd_init(
        opaque_vchi_instance_handle_t* initialise_instance, vchi_connection_t** connections, uint32_t num_connections);
    void vc_gencmd_stop(void);
    int vc_gencmd(char* response, int maxlen, const char* format, ...);

    // see "/opt/vc/include/interface/vmcs_host/vc_vchi_gpuserv.h"

    // these go in command word of gpu_job_s
    // EXECUTE_VPU and EXECUTE_QPU are valid from host
    enum
    {
        EXECUTE_NONE,
        EXECUTE_VPU,
        EXECUTE_QPU,
        EXECUTE_SYNC,
        LAUNCH_VPU1
    };

    struct qpu_job_s
    {
        // parameters for qpu job
        uint32_t jobs;
        uint32_t noflush;
        uint32_t timeout;
        uint32_t dummy;
        uint32_t control[12][2];
    };

    struct sync_job_s
    {
        // parameters for syncjob
        // bit 0 set means wait for preceding vpu jobs to complete
        // bit 1 set means wait for preceding qpu jobs to complete
        uint32_t mask;
        uint32_t dummy[27];
    };

    struct gpu_callback_s
    {
        // callback to call when complete (can be NULL)
        void (*func)();
        void* cookie;
    };

    struct gpu_internal_s
    {
        void* message;
        int refcount;
    };

    struct gpu_job_s
    {
        // from enum above
        uint32_t command;
        // qpu or vpu jobs
        union
        {
            // struct vpu_job_s v; // not of interest to us
            struct qpu_job_s q;
            struct sync_job_s s;
        } u;
        // callback function to call when complete
        struct gpu_callback_s callback;
        // for internal use - leave as zero
        struct gpu_internal_s internal;
    };

    int32_t vc_gpuserv_init(void);
    void vc_gpuserv_deinit(void);

    int32_t vc_gpuserv_execute_code(int num_jobs, struct gpu_job_s jobs[]);

#ifdef __cplusplus
}
#endif

VCHI::VCHI()
{
    if(vchi_initialise(&vchiHandle) != 0)
        throw std::runtime_error{"Failed to initialize VCHI interface!"};
    if(vchi_connect(nullptr, 0, vchiHandle) != 0)
        throw std::runtime_error{"Failed to connect to VCHI endpoint!"};
    if(vc_gpuserv_init() != 0)
        throw std::runtime_error{"Failed to initialize VCHI GPU service!"};
    vc_vchi_gencmd_init(vchiHandle, &connectionHandle, 1);
}

VCHI::~VCHI()
{
    vc_gencmd_stop();
    vc_gpuserv_deinit();
    vchi_disconnect(vchiHandle);
}

static std::atomic_uintptr_t currentJobId{0};
static std::mutex jobStatusLock;
std::condition_variable jobStatusCondition;
std::atomic_bool isJobDone{false};

static void onJobDone(uintptr_t jobId)
{
    // make sure we don't e.g. update the next job if our current one timed out
    std::lock_guard<std::mutex> guard(jobStatusLock);
    if(currentJobId == jobId)
    {
        isJobDone = true;
        jobStatusCondition.notify_all();
    }
}

ExecutionHandle VCHI::executeQPU(unsigned numQPUs, std::pair<uint32_t*, uint32_t> controlAddress, bool flushBuffer,
    std::chrono::milliseconds timeout) const
{
    if(timeout.count() > 0xFFFFFFFF)
    {
        DEBUG_LOG(DebugLevel::SYSCALL,
            std::cout << "Timeout is too big, needs fit into a 32-bit integer: " << timeout.count() << std::endl)
        return ExecutionHandle{false};
    }

    {
        std::lock_guard<std::mutex> guard(jobStatusLock);
        ++currentJobId;
        isJobDone = false;
    }

    gpu_job_s execJob{};
    execJob.command = EXECUTE_QPU;
    execJob.u.q.jobs = numQPUs;
    execJob.u.q.noflush = !flushBuffer;
    execJob.u.q.timeout = static_cast<uint32_t>(timeout.count());
    uint32_t* addressBase = controlAddress.first;
    for(unsigned i = 0; i < numQPUs; ++i)
    {
        // layout is similar to V3D registers, per-QPU UNIFORM address and code address
        execJob.u.q.control[i][0] = addressBase[0];
        execJob.u.q.control[i][1] = addressBase[1];
        addressBase += 2;
    }
    execJob.callback.func = reinterpret_cast<void (*)()>(onJobDone);
    execJob.callback.cookie = reinterpret_cast<void*>(currentJobId.load());

    auto start = std::chrono::high_resolution_clock::now();

    if(vc_gpuserv_execute_code(1, &execJob) != 0)
        return ExecutionHandle{false};

    auto checkFunc = [start, timeout]() -> bool {
        std::unique_lock<std::mutex> guard(jobStatusLock);
        // can't use wait_for here, since this function is not called immediately and putting the start to the start of
        // the function would extend our timeout
        jobStatusCondition.wait_until(guard, start + timeout, []() -> bool { return isJobDone; });
        return isJobDone;
    };
    return ExecutionHandle{checkFunc};
}

template <std::size_t N>
static std::string extractValue(const std::array<char, N>& buffer)
{
    auto pos = std::find(buffer.begin(), buffer.end(), '=');
    if(pos == buffer.end())
        return "";
    return pos + 1 /* skip '=' */;
}

bool VCHI::readValue(SystemQuery query, uint32_t& output) noexcept
{
    std::array<char, 128> buffer{};
    switch(query)
    {
    case SystemQuery::NUM_QPUS:
    case SystemQuery::TOTAL_VPM_MEMORY_IN_BYTES:
        return false;
    case SystemQuery::MAXIMUM_QPU_CLOCK_RATE_IN_HZ:
        if(vc_gencmd(buffer.data(), buffer.size(), "get_config gpu_freq") == 0)
        {
            // result is e.g. "gpu_freq=<value>"
            output = static_cast<uint32_t>(std::stoul(extractValue(buffer)) * 1000 * 1000);
            return true;
        }
        return false;
    case SystemQuery::CURRENT_QPU_CLOCK_RATE_IN_HZ:
        if(vc_gencmd(buffer.data(), buffer.size(), "measure_clock v3d") == 0)
        {
            // result is e.g. "frequency(46)=<value>"
            output = static_cast<uint32_t>(std::stoul(extractValue(buffer)));
            return true;
        }
        return false;
    case SystemQuery::MAXIMUM_ARM_CLOCK_RATE_IN_HZ:
        if(vc_gencmd(buffer.data(), buffer.size(), "get_config arm_freq") == 0)
        {
            // result is e.g. "arm_freq=<value>"
            output = static_cast<uint32_t>(std::stoul(extractValue(buffer)) * 1000 * 1000);
            return true;
        }
        return false;
    case SystemQuery::CURRENT_ARM_CLOCK_RATE_IN_HZ:
        if(vc_gencmd(buffer.data(), buffer.size(), "measure_clock arm") == 0)
        {
            // result is e.g. "frequency(48)=<value>"
            output = static_cast<uint32_t>(std::stoul(extractValue(buffer)));
            return true;
        }
        return false;
    case SystemQuery::TOTAL_ARM_MEMORY_IN_BYTES:
        if(vc_gencmd(buffer.data(), buffer.size(), "get_mem arm") == 0)
        {
            // result is e.g. "arm=896M"
            output = static_cast<uint32_t>(std::stoul(extractValue(buffer)) * 1024 * 1024);
            return true;
        }
        return false;
    case SystemQuery::TOTAL_GPU_MEMORY_IN_BYTES:
        if(vc_gencmd(buffer.data(), buffer.size(), "get_mem gpu") == 0)
        {
            // result is e.g. "gpu=128M"
            output = static_cast<uint32_t>(std::stoul(extractValue(buffer)) * 1024 * 1024);
            return true;
        }
        return false;
    case SystemQuery::QPU_TEMPERATURE_IN_MILLI_DEGREES:
        if(vc_gencmd(buffer.data(), buffer.size(), "measure_temp") == 0)
        {
            // result is e.g. "temp=39.2'C"
            auto tmp = std::stof(extractValue(buffer)) * 1000.0f;
            output = static_cast<uint32_t>(tmp);
            return true;
        }
        return false;
    }
    return false;
}
