/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_VCSM_H
#define VC4CL_VCSM_H

#include "hal.h"

namespace vc4cl
{
    /**
     * RAII wrapper around memory management via the VCSM (or VCSM-CMA) interfaces)
     */
    class VCSM
    {
    public:
        VCSM(bool useCMA);
        ~VCSM();

        std::unique_ptr<DeviceBuffer> allocateBuffer(const std::shared_ptr<SystemAccess>& system, unsigned sizeInBytes,
            const std::string& name, CacheType cacheType);
        bool deallocateBuffer(const DeviceBuffer* buffer);

        bool flushCPUCache(const DeviceBuffer* buffer);
        bool clearCPUCache(const DeviceBuffer* buffer);

        uint32_t getTotalGPUMemory() const;

        inline bool isUsingCMA()
        {
            return usesCMA;
        }

    private:
        bool usesCMA;
    };

} /* namespace vc4cl */

#endif /* VC4CL_VCSM_H */