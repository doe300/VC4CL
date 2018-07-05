/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifdef COMPILER_HEADER
	#define CPPLOG_NAMESPACE logging
	#include COMPILER_HEADER
#endif

#include "Program.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>

static void printGlobalData(std::istream& in, std::ostream& out, unsigned numBytes)
{
	out << "//Global data segment with " << numBytes << " bytes: " << std::endl;
	uint64_t data;
	for(unsigned i = 0; i < numBytes; i += sizeof(data))
	{
		in.read(reinterpret_cast<char*>(&data), sizeof(data));
		std::array<char, 64> buffer{};
		snprintf(buffer.data(), buffer.size(), "0x%08x 0x%08x", static_cast<uint32_t>(data & 0xFFFFFFFFLL), static_cast<uint32_t>((data & 0xFFFFFFFF00000000LL) >> 32));
		out << std::string(buffer.data()) << std::endl;
	}
	out << std::endl;
}

#if HAS_COMPILER
static void printInstructions(std::istream& in, std::ostream& out, unsigned numInstructions)
{
	out << "//Instructions segment with " << numInstructions << " instructions:" << std::endl;
	vc4c::disassembleCodeOnly(in, out, numInstructions, vc4c::OutputMode::HEX);
	out << std::endl;
}
#endif

static void printUniforms(std::istream& in, std::ostream& out, unsigned globalDataPointer, unsigned codePointer, unsigned uniformBasePointer, uint16_t uniformsPerIteration, uint16_t numIterations, vc4cl::KernelUniforms uniformsSet)
{
	out << "//UNIFORMS segment:" << std::endl;
	uint16_t qpuIndex = 0;
	unsigned val;
	while(in.read(reinterpret_cast<char*>(&val), sizeof(val)) && val != uniformBasePointer)
	{
		//the UNIFORM base-pointer is located behind all the UNIFORMs and marks their end
		out << "//UNIFORMS for QPU " << qpuIndex << ":" << std::endl;
		for(uint16_t i = 0; i < numIterations; ++i)
		{
			out << "//Iteration " << i << ":" << std::endl;
			//the first val is already read in while-loop
			if(uniformsSet.getWorkDimensionsUsed())
			{
				out << val << "\t\t//Work-dimensions" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getLocalSizesUsed())
			{
				out << val << "\t\t//Local sizes (" << (val & 0xFF) << ", " << ((val >> 8) & 0xFF) << ", " << ((val >> 16) & 0xFF) << ")" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getLocalIDsUsed())
			{
				out << val << "\t\t//Local IDs (" << (val & 0xFF) << ", " << ((val >> 8) & 0xFF) << ", " << ((val >> 16) & 0xFF) << ")" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getNumGroupsXUsed())
			{
				out << val << "\t\t//Num groups X" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getNumGroupsYUsed())
			{
				out << val << "\t\t//Num groups Y" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getNumGroupsZUsed())
			{
				out << val << "\t\t//Num groups Z" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGroupIDXUsed())
			{
				out << val << "\t\t//Group ID X" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGroupIDYUsed())
			{
				out << val << "\t\t//Group ID Y" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGroupIDZUsed())
			{
				out << val << "\t\t//Group ID Z" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGlobalOffsetXUsed())
			{
				out << val << "\t\t//Global offset X" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGlobalOffsetYUsed())
			{
				out << val << "\t\t//Global offset Y" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGlobalOffsetZUsed())
			{
				out << val << "\t\t//Global offset Z" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			if(uniformsSet.getGlobalDataAddressUsed())
			{
				out << "0x" << std::hex << val << std::dec << "\t//Global data pointer" << std::endl;
				if(val != globalDataPointer)
					out << "ERROR: Global data pointer deviates from base-pointer 0x" << std::hex << globalDataPointer << std::dec << "!" << std::endl;
				out << "//Kernel parameters (" << (uniformsPerIteration - uniformsSet.countUniforms() - 1 /* re-run flag */) << " UNIFORMs):" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			for(uint16_t u = uniformsSet.countUniforms() + 1 /* re-run flag */; u < uniformsPerIteration; ++u)
			{
				out << "0x" << std::hex << val << " (" << std::dec << val << ")" << std::endl;
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
			}
			out << val << "\t\t//Work-group repeat flag (" << (val > 0 ? "repeat" : "end") << ")" << std::endl;

			//read work-dimensions for next iteration
			if(i + 1 < numIterations)
				in.read(reinterpret_cast<char*>(&val), sizeof(val));
		}
		++qpuIndex;
		out << std::endl;
	}
	out << "//Read UNIFORMs for " << qpuIndex << " QPUs" << std::endl;
	out << std::endl;
}

int main(int argc, char** argv)
{
	if(argc != 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 )
	{
		std::cout << "Usage: vc4cl_dump_analyzer <input-file>" << std::endl;
		return 0;
	}
#if HAS_COMPILER

	std::ifstream f(argv[1], std::ios_base::in|std::ios_base::binary);

	//For the layout of the dump-file, see executor.cpp#executeKernel
	/*
	 * +------------------------|
	 * | QPU base pointer       |------------+
	 * | (global data pointer)  |            |
	 * | QPU code pointer       |----------+ |
	 * | QPU UNIFORM pointer    |--------+ | |
	 * | #UNIFORMS | iterations |        | | |
	 * +------------------------+<-------|-|-+
	 * |  Data Segment          |        | |
	 * +------------------------+ <----+-|-+
	 * |  QPU Code              |      | |
	 * |  ...                   |      | |
	 * +------------------------+ <--+-|-+
	 * |  Uniforms              |    | |
	 * +------------------------+    | |
	 * |  QPU0 Uniform --------------+ |
	 * |  QPU0 Start   ----------------+
	 * +------------------------+
	 */

	unsigned qpuBasePointer;
	unsigned qpuCodePointer;
	unsigned qpuUniformPointer;
	uint16_t numUniformsPerIteration;
	uint16_t numIterations;
	unsigned implicitUniformBitset;

	f.read(reinterpret_cast<char*>(&qpuBasePointer), sizeof(unsigned));
	f.read(reinterpret_cast<char*>(&qpuCodePointer), sizeof(unsigned));
	f.read(reinterpret_cast<char*>(&qpuUniformPointer), sizeof(unsigned));
	f.read(reinterpret_cast<char*>(&numUniformsPerIteration), sizeof(uint16_t));
	f.read(reinterpret_cast<char*>(&numIterations), sizeof(uint16_t));
	f.read(reinterpret_cast<char*>(&implicitUniformBitset), sizeof(unsigned));
	vc4cl::KernelUniforms uniformsSet;
	uniformsSet.value = implicitUniformBitset;

	//sizes in bytes
	unsigned globalDataSize = qpuCodePointer - qpuBasePointer;
	unsigned codeSize = qpuUniformPointer - qpuCodePointer;
	unsigned numInstructions = codeSize / sizeof(uint64_t);

	printGlobalData(f, std::cout, globalDataSize);
	printInstructions(f, std::cout, numInstructions);
	printUniforms(f, std::cout, qpuBasePointer, qpuCodePointer, qpuUniformPointer, numUniformsPerIteration, numIterations, uniformsSet);

#else
	std::cerr << "For the dump analyzer to work, VC4CL needs to be compiled with VC4C support!" << std::endl;
	return 1;
#endif
}
