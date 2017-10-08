
## Benchmarks

**TODO**
Run benchmarks, compare results to other devices

* [CLPeak](https://github.com/krrishnarraj/clpeak): default settings do not work (see [issue](https://github.com/krrishnarraj/clpeak/issues/41)), runs single precision, global bandwidth, transfer bandwidth for modified work-sizes
* [mixbench](https://github.com/ekondis/mixbench): default work-group size of 256, can be specified as parameter. Hangs on throwing an CompilationError in pre-compilation (see [this ??](https://stackoverflow.com/questions/37636214/programs-hangs-while-throwing-an-exception))
* [OpenDwarfs](https://github.com/vtsynergy/OpenDwarfs): compilation errors in benchmark code
* [PolyBench](https://github.com/cavazos-lab/PolyBench-ACC): tested some benchmarks, partially working, not so meaningful output
* [SHOC](https://github.com/vetter/shoc): Some tests require bigger work-group sizes, other have compilation errors
* http://viennaclbench.sourceforge.net/
* [FinanceBench](https://github.com/cavazos-lab/FinanceBench): Only benchmarks Black-Scholes and Monte-Carlo support OpenCL. Huge speedup (too much?? 2000-times!) on GPU, but no/wrong results, Mote-Carlo fails to compile (long)
* http://www.bealto.com/gpu-benchmarks_intro.html: crashes at/after CPU-side memory benchmark (copyN)

* [gearshifft](https://github.com/mpicbg-scicomp/gearshifft)
* http://www.luxmark.info/
* https://openbenchmarking.org/test/pts/bullet ??
* [chai](https://github.com/chai-benchmarks/chai)