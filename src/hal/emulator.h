/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_EMULATOR
#define VC4CL_EMULATOR

#include "hal.h"

namespace vc4cl
{
    uint32_t getTotalEmulatedMemory();
    std::unique_ptr<DeviceBuffer> allocateEmulatorBuffer(const std::shared_ptr<SystemAccess>& system, unsigned sizeInBytes);
    bool deallocateEmulatorBuffer(const DeviceBuffer* buffer);

    uint8_t getNumEmulatedQPUs();
    uint32_t getEmulatedQPUClockRateInHz();
    uint32_t getEmulatedGPUTemperatureInMilliDegree();
    uint32_t getTotalEmulatedVPMMemory();

    bool emulateQPU(unsigned numQPUs, uint32_t bufferQPUAddress, std::chrono::milliseconds timeout);

} /* namespace vc4cl */

#endif /* VC4CL_EMULATOR */
