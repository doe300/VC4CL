## Security Considerations
Via the DMA-interface and because there is no MMU between the GPU and the RAM, code executed on the GPU can **access any part of the main memory**!
This means, an OpenCL kernel could be used to read sensitive data or write into kernel memory!