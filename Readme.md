## Readme

**VC4CL** is an implementation of the **OpenCL 1.2** standard for the VideoCore IV GPU (found in all Raspberry Pi models).

The implementation consists of:

* The **VC4CL** OpenCL runtime library, running on the host CPU to compile, run and intercat with OpenCL kernels.
* The **VC4C** compiler, converting OpenCL kernels into machine code. This compiler also provides an implementation of the OpenCL built-in functions.
* The **VC4CLStdLib**, the platform-specific implementation of the OpenCL C standard library, is linked in with the kernel by **VC4C**

## OpenCL-Support
The VC4CL implementation supports the **EMBEDDED PROFILE** of the [OpenCL standard version 1.2](https://Fwww.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf).
Aditionally the [`cl_khr_icd` extension](https://Fwww.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf) is supported, to allow VC4CL to be found by an installable client driver loader (**ICD**). This enables VC4CL to be used in parallel with another OpenCL implementation, e.g. [pocl](https://github.com/pocl/pocl), which executes OpenCL code on the host CPU.

The OpenCL version 1.2 was selected as target standard version, since it is the last version of the OpenCL standard where all mandatory features can be supported.

VC4CL supports the **EMBEDDED PROFILE** of the OpenCL-standard, which is a trimmed version of the default **FULL PROFILE**. The most notable features, which are not supported by the VC4CL implementation are the `long` data-type and partitioning devices. See [Standard Support](StandardSupport.md) for more details of (not) supported features.

## VideoCore IV GPU
The VideoCore IV GPU, in the configuration as found in the Raspberry Pi models, has a theoretical maximum performance of **24 GPFLOS** and is therefore very powerful in comparison to the host CPU.
The GPU (which is located on the same chip as the CPU) has 12 cores, able of running independent instructions each, supports a SIMD vector-width of 16 elements natively and can access the RAM directly via DMA.
