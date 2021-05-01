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

namespace vc4cl
{
    constexpr uint32_t PAGE_ALIGNMENT = 4096;

    class Mailbox;
    class V3D;
    class VCSM;

    enum class CacheType : uint8_t
    {
        UNCACHED = 0,
        HOST_CACHED = 1,
        GPU_CACHED = 2,
        BOTH_CACHED = 3
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
        uint32_t getTotalGPUMemory();
        uint8_t getNumQPUs();
        uint32_t getQPUClockRateInHz();
        uint32_t getGPUTemperatureInMilliDegree();
        uint32_t getTotalVPMMemory();

        inline Mailbox* getMailboxIfAvailable()
        {
            return mailbox.get();
        }

        inline V3D* getV3DIfAvailable()
        {
            return v3d.get();
        }

        std::unique_ptr<DeviceBuffer> allocateBuffer(
            unsigned sizeInBytes, const std::string& name, CacheType cacheType = CacheType::BOTH_CACHED);
        bool deallocateBuffer(const DeviceBuffer* buffer);

        CHECK_RETURN ExecutionHandle executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress,
            bool flushBuffer, std::chrono::milliseconds timeout);

        bool executesKernelsViaV3D() const;

    private:
        SystemAccess();

        std::unique_ptr<Mailbox> mailbox;
        std::unique_ptr<V3D> v3d;
        std::unique_ptr<VCSM> vcsm;

        friend std::shared_ptr<SystemAccess>& system();
    };

    std::shared_ptr<SystemAccess>& system();

} /* namespace vc4cl */

#endif /* VC4CL_HAL */