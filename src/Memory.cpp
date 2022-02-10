/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code. See the copyright statement below for the original
 * code:
 */
#include "Memory.h"

#include "hal/hal.h"

#include <iomanip>

using namespace vc4cl;

std::ostream& vc4cl::operator<<(std::ostream& s, const DevicePointer& ptr)
{
    return s << "0x" << std::hex << std::setfill('0') << std::setw(8) << ptr.pointer << std::dec << std::setfill(' ');
}

DeviceBuffer::DeviceBuffer(
    const std::shared_ptr<SystemAccess>& sys, uint32_t handle, DevicePointer devPtr, void* hostPtr, uint32_t size) :
    memHandle(handle),
    qpuPointer(devPtr), hostPointer(hostPtr), size(size), system(sys)
{
}

DeviceBuffer::~DeviceBuffer()
{
    if(memHandle != 0)
        system->deallocateBuffer(this);
}

void DeviceBuffer::dumpContent() const
{
    // TODO rewrite
    for(unsigned i = 0; i < size / sizeof(unsigned); ++i)
    {
        if((i % 8) == 0)
            printf("\n[VC4CL] %08x:", static_cast<unsigned>(static_cast<unsigned>(qpuPointer) + i * sizeof(unsigned)));
        printf(" %08x", reinterpret_cast<const unsigned*>(hostPointer)[i]);
    }
}
