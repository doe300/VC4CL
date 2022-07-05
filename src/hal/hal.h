/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_HAL
#define VC4CL_HAL

#include "../Memory.h"
#include "../executor.h"

#include <memory>
#include <vector>

namespace vc4cl
{
    constexpr uint32_t PAGE_ALIGNMENT = 4096;

    class Mailbox;
    class V3D;
    #ifndef NO_VCSM
    class VCSM;
    #endif
    class VCHI;

    enum class CacheType : uint8_t
    {
        UNCACHED = 0,
        HOST_CACHED = 1,
        GPU_CACHED = 2,
        BOTH_CACHED = 3
    };

    enum class ExecutionMode : uint8_t
    {
        MAILBOX_IOCTL,
        V3D_REGISTER_POKING,
        VCHI_GPU_SERVICE
    };

    enum class MemoryManagement : uint8_t
    {       
        MAILBOX,
	#ifndef NO_VCSM
        VCSM,
        VCSM_CMA
	#endif
    };

    enum class SystemQuery : uint8_t
    {
        NUM_QPUS,
        TOTAL_GPU_MEMORY_IN_BYTES,
        TOTAL_ARM_MEMORY_IN_BYTES,
        CURRENT_QPU_CLOCK_RATE_IN_HZ,
        MAXIMUM_QPU_CLOCK_RATE_IN_HZ,
        CURRENT_ARM_CLOCK_RATE_IN_HZ,
        MAXIMUM_ARM_CLOCK_RATE_IN_HZ,
        QPU_TEMPERATURE_IN_MILLI_DEGREES,
        TOTAL_VPM_MEMORY_IN_BYTES
    };

    /**
     * Abstraction for any system access.
     *
     * This abstraction allows for dynamic (or at least run-time) selection of the actual system access methods
     * (mailbox, VCSM, V3D registers, procfs, etc.) to be used and therefore facilitates debugability and comparison of
     * the different system access methods.
     */
    class SystemAccess : public std::enable_shared_from_this<SystemAccess>
    {
    public:
        inline uint32_t getTotalGPUMemory()
        {
            return querySystem(SystemQuery::TOTAL_GPU_MEMORY_IN_BYTES, 0);
        }

        inline uint8_t getNumQPUs()
        {
            return static_cast<uint8_t>(querySystem(SystemQuery::NUM_QPUS, 12));
        }

        inline uint32_t getCurrentQPUClockRateInHz()
        {
            return querySystem(SystemQuery::CURRENT_QPU_CLOCK_RATE_IN_HZ, 0);
        }

        inline uint32_t getMaximumQPUClockRateInHz()
        {
            return querySystem(SystemQuery::MAXIMUM_QPU_CLOCK_RATE_IN_HZ, 0);
        }

        inline uint32_t getGPUTemperatureInMilliDegree()
        {
            return querySystem(SystemQuery::QPU_TEMPERATURE_IN_MILLI_DEGREES, 0);
        }

        uint32_t getTotalVPMMemory();
        uint32_t querySystem(SystemQuery query, uint32_t defaultValue);

        std::string getModelType();
        std::string getProcessorType();

        inline Mailbox* getMailboxIfAvailable()
        {
            return mailbox.get();
        }

        inline V3D* getV3DIfAvailable()
        {
            return v3d.get();
        }
	#ifndef NO_VCSM
        inline VCSM* getVCSMIfAvailable()
        {
            return vcsm.get();
        }
	#endif

        inline VCHI* getVCHIIfAvailable()
        {
            return vchi.get();
        }

        std::unique_ptr<DeviceBuffer> allocateBuffer(
            unsigned sizeInBytes, const std::string& name, CacheType cacheType = CacheType::BOTH_CACHED);
        std::unique_ptr<DeviceBuffer> allocateGPUOnlyBuffer(
            unsigned sizeInBytes, const std::string& name, CacheType cacheType = CacheType::GPU_CACHED);
        bool deallocateBuffer(const DeviceBuffer* buffer);
        bool flushCPUCache(const std::vector<const DeviceBuffer*>& buffers);

        CHECK_RETURN ExecutionHandle executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress,
            bool flushBuffer, std::chrono::milliseconds timeout);

        const bool isEmulated;
        const ExecutionMode executionMode;
        const MemoryManagement memoryManagement;
        const std::pair<bool, CacheType> forcedCacheType;

    private:
        SystemAccess();

        std::unique_ptr<Mailbox> mailbox;
        std::unique_ptr<V3D> v3d;
	#ifndef NO_VCSM
        std::unique_ptr<VCSM> vcsm;
	#endif
        std::unique_ptr<VCHI> vchi;

        friend std::shared_ptr<SystemAccess>& system();
    };

    std::shared_ptr<SystemAccess>& system();

} /* namespace vc4cl */

#endif /* VC4CL_HAL */
