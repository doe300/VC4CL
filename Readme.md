# Status

[![CircleCI](https://circleci.com/gh/nomaddo/VC4CL.svg?style=svg)](https://circleci.com/gh/nomaddo/VC4CL)

# VC4CL

**VC4CL** is an implementation of the **OpenCL 1.2** standard for the VideoCore IV GPU (found in all Raspberry Pi models).

The implementation consists of:

* The **VC4CL** OpenCL runtime library, running on the host CPU to compile, run and intercat with OpenCL kernels.
* The **[VC4C](https://github.com/doe300/VC4C)** compiler, converting OpenCL kernels into machine code. This compiler also provides an implementation of the OpenCL built-in functions.
* The **[VC4CLStdLib](https://github.com/doe300/VC4CLStdLib)**, the platform-specific implementation of the OpenCL C standard library, is linked in with the kernel by **VC4C**

## OpenCL-Support
The VC4CL implementation supports the **EMBEDDED PROFILE** of the [OpenCL standard version 1.2](https://Fwww.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf).
Additionally the [`cl_khr_icd` extension](https://Fwww.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf) is supported, to allow VC4CL to be found by an installable client driver loader (**ICD**). This enables VC4CL to be used in parallel with another OpenCL implementation, e.g. [pocl](https://github.com/pocl/pocl), which executes OpenCL code on the host CPU.

The OpenCL version 1.2 was selected as target standard version, since it is the last version of the OpenCL standard where all mandatory features can be supported.

VC4CL supports the **EMBEDDED PROFILE** of the OpenCL-standard, which is a trimmed version of the default **FULL PROFILE**. The most notable features, which are not supported by the VC4CL implementation are images, the `long` and `double` data-types, device-side `printf` and partitioning devices as well as linking of source files. See [RuntimeLibrary](doc/RuntimeLibrary.md) for more details of (not) supported features.

## VideoCore IV GPU
The VideoCore IV GPU, in the configuration as found in the Raspberry Pi models, has a theoretical maximum performance of **24 GPFLOS** and is therefore very powerful in comparison to the host CPU.
The GPU (which is located on the same chip as the CPU) has 12 cores, able of running independent instructions each, supports a SIMD vector-width of 16 elements natively and can access the RAM directly via DMA.

## Required software

- A C++11-capable compiler
- The [VC4C](https://github.com/doe300/VC4C) compiler to compile OpenCL C-code
- The Khronos ICD Loader (available in the official Raspbian repository as `sudo apt-get install ocl-icd-opencl-dev`) for building with ICD-support (e.g. allows to run several OpenCL implementations on one machine)
- The OpenCL headers in version >= 1.2 (available in the Raspbian repositories as `sudo apt-get install opencl-headers`)
 

## Build

The following configuration options are available in CMake:

- `BUILD_TESTING` toggles building of test program (when configured, can be built with `make TestVC4CL`)
- `BUILD_DEBUG` toggles building debug or release program
- `CROSS_COMPILE` toggles whether to cross-compile for the Raspberry Pi, requires the [Raspberry Pi cross-compiler](https://github.com/raspberrypi/tools) to be installed
- `CROSS_COMPILER_PATH` sets the root path to the Raspberry Pi cross compiler, defaults to `/opt/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64` (e.g. for the cross compiler cloned into the directory `/opt/raspberrypi/tools/`)
- `INCLUDE_COMPILER` whether to include the [VC4C](https://github.com/doe300/VC4C) compiler
- `VC4C_HEADER_PATH` sets the path to the VC4C include headers, defaults to `../VC4C/include/VC4C.h` or `lib/vc4c/include/VC4C.h`
- `VC4CC_LIBRARY` sets the path to the VC4C compiler library, defaults to `../VC4C/build/libVC4CC.xxx` or `lib/vc4c/build/libVC4CC.xxx`
- `BUILD_ICD` toggles whether to build with support for the Khronos ICD loader, requires the ICD loader to be installed system-wide
- `IMAGE_SUPPORT` toggles whether to enable the very experimental image-support
- `REGISTER_POKE_KERNELS` toggles the use of register-poking to start kernels (if disabled, uses the mailbox system-calls). Enabling this increases performance up to 10%, but may crash the system, if any other application accesses the GPU at the same time!

## Khronos ICD Loader
The Khronos ICD Loaders allows multiple OpenCL implementation to be used in parallel (e.g. VC4CL and [pocl](https://github.com/pocl/pocl)), but requires a bit of manual configuration:
Create a file `/etc/OpenCL/vendors/VC4CL.icd` with a single line containing the absolute path to the VC4CL library.

The program clinfo can be used to test, whether the ICD loader finds the VC4CL implementation. **Note**: the program version in the official Raspbian repository is too old and has a bug (see [fix](https://github.com/Oblomov/clinfo/commit/4728656fcb1ff5d506b8ef2103af83ce11ceae36)), so it must be compiled from the [github](https://github.com/Oblomov/clinfo) repository.

## Security Considerations
Because of the DMA-interface which has no MMU between the GPU and the RAM, code executed on the GPU can **access any part of the main memory**!
This means, an OpenCL kernel could be used to read sensitive data or write into kernel memory!

**Therefore, any program using the VC4CL implementation must be run as root!**
