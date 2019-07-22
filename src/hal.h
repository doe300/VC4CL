/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_HAL
#define VC4CL_HAL 1

#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <functional>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // see "/opt/vc/include/bcm_host.h"
    void bcm_host_init(void);
    void bcm_host_deinit(void);
    unsigned bcm_host_get_peripheral_address(void);

#ifdef __cplusplus
}
#endif

namespace vc4cl
{
#if defined(MOCK_HAL) && MOCK_HAL

    int open_mailbox(const char* path, int flags);
    int close_mailbox(int fd);
    int ioctl_mailbox(int fd, unsigned long request, void* data);

    int open_memory();
    int close_memory(int fd);
    void* map_memory(void* addr, size_t len, int prot, int flags, int fd, off_t offset);
    int unmap_memory(void* addr, size_t len);

    struct WordWrapper
    {
        uint32_t word;
        std::function<uint32_t(uint32_t)> onWrite = [](uint32_t val) -> uint32_t { return val; };

        inline explicit operator uint32_t() const
        {
            return word;
        }

        inline uint32_t operator>>(uint32_t offset) const
        {
            return word >> offset;
        }

        inline uint32_t operator&(uint32_t mask) const
        {
            return word & mask;
        }

        inline uint32_t operator=(uint32_t newVal)
        {
            word = onWrite(newVal);
            return word;
        }

        inline uint32_t operator&=(uint32_t mask)
        {
            word = onWrite(word & mask);
            return word;
        }

        inline uint32_t operator|=(uint32_t mask)
        {
            word = onWrite(word | mask);
            return word;
        }
    };

    WordWrapper& v3d_register(uint32_t* basePtr, uint32_t offset);

#else
    inline int open_mailbox(const char* path, int flags)
    {
        return open(path, flags);
    }

    inline int close_mailbox(int fd)
    {
        return close(fd);
    }

    inline int ioctl_mailbox(int fd, unsigned long request, void* data)
    {
        return ioctl(fd, request, data);
    }

    inline int open_memory()
    {
        return open("/dev/mem", O_RDWR | O_SYNC);
    }

    inline int close_memory(int fd)
    {
        return close(fd);
    }

    inline void* map_memory(void* addr, size_t len, int prot, int flags, int fd, off_t offset)
    {
        return mmap(addr, len, prot, flags, fd, offset);
    }

    inline int unmap_memory(void* addr, size_t len)
    {
        return munmap(addr, len);
    }

    inline uint32_t& v3d_register(uint32_t* basePtr, uint32_t offset)
    {
        return basePtr[offset];
    }

#endif

    // The offsets are byte-offsets, but we need to convert them to 32-bit word offsets
    constexpr uint32_t V3D_IDENT0 = 0x0000 / sizeof(uint32_t);
    constexpr uint32_t V3D_IDENT1 = 0x0004 / sizeof(uint32_t);
    constexpr uint32_t V3D_L2CACTL = 0x00020 / sizeof(uint32_t);
    constexpr uint32_t V3D_SLCACTL = 0x00024 / sizeof(uint32_t);
    // constexpr uint32_t V3D_IDENT2 = 0x0008 / sizeof(uint32_t);
    constexpr uint32_t V3D_QPU_RESERVATIONS0 = 0x0410 / sizeof(uint32_t);
    // constexpr uint32_t V3D_QPU_RESERVATIONS1 = 0x0414 / sizeof(uint32_t);
    constexpr uint32_t V3D_SRQPC = 0x00430 / sizeof(uint32_t);
    constexpr uint32_t V3D_SRQUA = 0x00434 / sizeof(uint32_t);
    constexpr uint32_t V3D_SRQCS = 0x0043c / sizeof(uint32_t);
    constexpr uint32_t V3D_VPMBASE = 0x00504 / sizeof(uint32_t);
    constexpr uint32_t V3D_COUNTER_CLEAR = 0x0670 / sizeof(uint32_t);
    constexpr uint32_t V3D_COUNTER_ENABLE = 0x0674 / sizeof(uint32_t);
    constexpr uint32_t V3D_COUNTER_VALUE_BASE = 0x0680 / sizeof(uint32_t);
    constexpr uint32_t V3D_COUNTER_MAPPING_BASE = 0x0684 / sizeof(uint32_t);
    constexpr uint32_t V3D_ERRORS = 0x0F20 / sizeof(uint32_t);
    // offset between two counter-enable, two counter-value or two counter-mapping registers
    constexpr uint32_t V3D_COUNTER_INCREMENT = 0x0008 / sizeof(uint32_t);
    constexpr uint32_t V3D_LENGTH = ((V3D_ERRORS - V3D_IDENT0) + 16) * sizeof(uint32_t);
} /* namespace vc4cl */

#endif /* VC4CL_HAL */
