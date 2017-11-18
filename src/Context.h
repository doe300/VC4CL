/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_CONTEXT_H
#define VC4CL_CONTEXT_H

#include "Device.h"
#include "Platform.h"

namespace vc4cl
{
	using ContextCallback = void(CL_CALLBACK*)(const char* errinfo, const void *private_info, size_t cb, void* user_data);

	enum ContextProperty
	{
		NONE = 0,
		USER_SYNCHRONISATION = 1,
		PLATFORM = 2
	};

	class Context : public Object<_cl_context, CL_INVALID_CONTEXT>
	{
	public:
		Context(const Device* device, bool userSync, const Platform* platform, ContextProperty explicitProperties, ContextCallback callback = nullptr, void* userData = nullptr);
		~Context() override;
		CHECK_RETURN cl_int getInfo(cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

		void fireCallback(const std::string& errorInfo, const void* privateInfo, size_t cb);

		const Device* device;

	private:
		//properties
		const bool userSync;
		const Platform* platform;
		const ContextProperty explicitProperties;

		//callback
		const ContextCallback callback;
		void* userData;
	};

	class HasContext
	{
	public:
		explicit HasContext(Context* context);
		virtual ~HasContext();

		const Context* context() const;
		Context* context();

	private:
		object_wrapper<Context> c;
	};

} /* namespace vc4cl */

#endif /* VC4CL_CONTEXT_H */
