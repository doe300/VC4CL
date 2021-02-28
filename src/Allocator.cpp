/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#include "Allocator.h"

using namespace vc4cl;

DeviceBlock::DeviceBlock(const std::shared_ptr<Mailbox>& mb, uint32_t handle, DevicePointer devPtr, void* hostPtr,
    uint32_t numBytes, MemoryFlag flags) :
    flags(flags),
    memHandle(handle), qpuPointer(devPtr), hostPointer(hostPtr), size(numBytes), mailbox(mb), freeOffset(0)
{
}

DeviceBlock::~DeviceBlock() noexcept
{
    mailbox->deallocateBlock(memHandle, hostPointer, qpuPointer, size);
}

std::unique_ptr<DeviceBuffer> DeviceBlock::allocateBuffer(unsigned sizeInBytes, unsigned alignmentInBytes)
{
    // Monotonic allocator, if the requested size fits, allocate and set new free pointer
    std::size_t space = size - freeOffset;
    void* freePointer = reinterpret_cast<char*>(hostPointer) + freeOffset;
    auto alignedFreePointer = std::align(alignmentInBytes, sizeInBytes, freePointer, space);
    if(!alignedFreePointer ||
        (reinterpret_cast<char*>(hostPointer) + size) < (reinterpret_cast<char*>(alignedFreePointer) + sizeInBytes))
        // does not fit, abort
        return nullptr;

    freeOffset = static_cast<uint32_t>(size - space);
    auto deviceAddress = static_cast<unsigned>(qpuPointer) + freeOffset;
    std::unique_ptr<DeviceBuffer> buffer(
        new DeviceBuffer(shared_from_this(), DevicePointer{deviceAddress}, alignedFreePointer, sizeInBytes));
    DEBUG_LOG(DebugLevel::DEVICE_MEMORY,
        std::cout << "Reserved " << sizeInBytes << " bytes of buffer: device address " << std::hex << "0x"
                  << deviceAddress << ", host address " << alignedFreePointer << std::dec << " (offset " << freeOffset
                  << ", alignment " << alignmentInBytes << ')' << std::endl)
    freeOffset += sizeInBytes;
    return buffer;
}

void DeviceBlock::deallocateBuffer(void* ptr, uint32_t numBytes)
{
    // nothing to do here for monotonic allocators
}
