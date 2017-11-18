/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_SVM
#define VC4CL_SVM

#include "Context.h"
#include "Event.h"
#include "Mailbox.h"

#include <memory>

namespace vc4cl
{

	/*!
	 *
	 * OpenCL 2.0 specification, page 167:
	 * "OpenCL 2.0 adds support for shared virtual memory (a.k.a. SVM). SVM allows the host and kernels executing on devices to directly share complex, pointer-containing data structures such as trees and linked lists.
	 * It also eliminates the need to marshal data between the host and devices. As a result, SVM substantially simplifies OpenCL programming and may improve performance."
	 *
	 * We support fine-grained buffer sharing:
	 * "Fine-grained sharing: Shared virtual memory where memory consistency is maintained at a granularity smaller than a buffer.[...]
	 *  If SVM atomic operations are not supported, the host and devices can concurrently read the same memory locations and can concurrently update non-overlapping memory regions, but attempts to update the same memory
	 *  locations are undefined.  Memory consistency is guaranteed at synchronization points[...]"
	 * "Fine-grain buffer sharing provides fine-grain SVM only within buffers and is an extension of coarse-grain sharing[...]"
	 *
	 * The ARM extension cl_arm_shared_virtual_memory ports these features to OpenCL 1.2
	 */
	class SharedVirtualMemory : public HasContext
	{
	public:
		SharedVirtualMemory(Context* context, std::shared_ptr<DeviceBuffer> buffer);
		~SharedVirtualMemory() override;

		cl_int getHostOffset(const void* hostPointer) const;
		void* getDevicePointer(size_t offset = 0);
		void* getHostPointer(size_t offset = 0);

		static SharedVirtualMemory* findSVM(const void* hostPtr);
	private:
		std::shared_ptr<DeviceBuffer> buffer;
	};

	struct SVMMemcpy : public EventAction
	{
		const void* sourcePtr;
		void* destPtr;
		std::size_t numBytes;

		SVMMemcpy(const void* src, void* dest, std::size_t numBytes);

		cl_int operator()(Event* event) override;
	};

	struct SVMFill : public SVMMemcpy
	{
		std::vector<char> pattern;

		SVMFill(void* dest, const void* pattern, std::size_t patternSize, std::size_t numBytes);

		cl_int operator()(Event* event) override;
	};

} /* namespace vc4cl */

#endif /* VC4CL_SVM */
