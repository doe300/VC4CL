/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "userland.h"

#include <dlfcn.h>
#include <string>
#include <system_error>
#include <vector>

// libbcm_host.so
// - bcm_host_init
// - bcm_host_get_processor_id
// - bcm_host_get_model_type
// - bcm_host_deinit
// - bcm_host_get_peripheral_address
// - vc_gencmd
// - vc_gencmd_stop
// - vc_gpuserv_init
// - vc_gpuserv_execute_code
// - vc_gpuserv_deinit
// -> libvchiq_arm.so
// - vchi_initialise
// - vchi_connect
// - vchi_disconnect
// -> libvcos.so
// libvcsm.so
// - vcsm_init_ex
// - vcsm_malloc_cache
// - vcsm_lock
// - vcsm_unlock_ptr
// - vcsm_free
// - vcsm_clean_invalid2
// - vcsm_vc_addr_from_hdl
// - vcsm_exit
// -> libvcos.so

// The firmware libraries were moved in Raspberry Pi OS Bullseye from /opt/vc/lib/ to /usr/lib/arm-linux-gnueabihf/
static const std::vector<std::string> lookupPaths = {"/opt/vc/lib/", "/usr/lib/arm-linux-gnueabihf/"};

struct LibraryHandle
{
    explicit LibraryHandle(void* handle = nullptr) : handle(handle) {}
    LibraryHandle(const LibraryHandle&) = delete;
    LibraryHandle(LibraryHandle&& other) noexcept : handle(other.handle)
    {
        other.handle = nullptr;
    }

    ~LibraryHandle() noexcept
    {
        if(handle)
        {
            dlclose(handle);
        }
    }

    LibraryHandle& operator=(const LibraryHandle&) = delete;
    LibraryHandle& operator=(LibraryHandle&&) noexcept = delete;

    template <typename Signature>
    Signature* lookup(const char* name) const noexcept
    {
        return reinterpret_cast<Signature*>(dlsym(handle, name));
    }

    operator bool() const noexcept
    {
        return handle;
    }

    void* handle;
};

static LibraryHandle resolveLibrary(const std::string& name)
{
    for(const auto& path : lookupPaths)
    {
        if(auto handle = dlopen((path + name).data(), RTLD_NOW | RTLD_GLOBAL))
        {
            return LibraryHandle{handle};
        }
    }
    return LibraryHandle{};
}

template <typename Signature>
static Signature* resolveBcmHostLibrarySymbol(const char* name)
{
    static auto bcmHostLibrary = resolveLibrary("libbcm_host.so");
    if(!bcmHostLibrary)
    {
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory), "Could not find supported libbcm_host.so");
    }
    if(auto symbol = bcmHostLibrary.lookup<Signature>(name))
        return symbol;
    throw std::system_error{std::make_error_code(std::errc::function_not_supported),
        "Could not find function '" + std::string{name} + "' in libbcm_host.so"};
}

template <typename Signature>
static Signature* resolveVCSMLibrarySymbol(const char* name)
{
    static auto vcsmLibrary = resolveLibrary("libvcsm.so");
    if(!vcsmLibrary)
    {
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory), "Could not find supported libvcsm.so");
    }
    if(auto symbol = vcsmLibrary.lookup<Signature>(name))
        return symbol;
    throw std::system_error{std::make_error_code(std::errc::function_not_supported),
        "Could not find function '" + std::string{name} + "' in libvcsm.so"};
}

void bcm_host_init()
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(bcm_host_init)>("bcm_host_init");
    return func();
}

void bcm_host_deinit()
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(bcm_host_deinit)>("bcm_host_deinit");
    return func();
}

unsigned bcm_host_get_peripheral_address()
{
    static auto func =
        resolveBcmHostLibrarySymbol<decltype(bcm_host_get_peripheral_address)>("bcm_host_get_peripheral_address");
    return func();
}

int bcm_host_get_model_type(void)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(bcm_host_get_model_type)>("bcm_host_get_model_type");
    return func();
}

int bcm_host_get_processor_id(void)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(bcm_host_get_model_type)>("bcm_host_get_model_type");
    return func();
}

int vcsm_init_ex(int want_cma, int fd)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_init_ex)>("vcsm_init_ex");
    return func(want_cma, fd);
}

void vcsm_exit(void)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_exit)>("vcsm_exit");
    return func();
}

unsigned int vcsm_malloc_cache(unsigned int size, VCSM_CACHE_TYPE_T cache, const char* name)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_malloc_cache)>("vcsm_malloc_cache");
    return func(size, cache, name);
}

void vcsm_free(unsigned int handle)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_free)>("vcsm_free");
    return func(handle);
}

unsigned int vcsm_vc_addr_from_hdl(unsigned int handle)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_vc_addr_from_hdl)>("vcsm_vc_addr_from_hdl");
    return func(handle);
}

void* vcsm_lock(unsigned int handle)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_lock)>("vcsm_lock");
    return func(handle);
}

int vcsm_unlock_ptr(void* usr_ptr)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_unlock_ptr)>("vcsm_unlock_ptr");
    return func(usr_ptr);
}

int vcsm_export_dmabuf(unsigned int vcsm_handle)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_export_dmabuf)>("vcsm_export_dmabuf");
    return func(vcsm_handle);
}

int vcsm_clean_invalid2(struct vcsm_user_clean_invalid2_s* s)
{
    static auto func = resolveVCSMLibrarySymbol<decltype(vcsm_clean_invalid2)>("vcsm_clean_invalid2");
    return func(s);
}

int32_t vchi_initialise(opaque_vchi_instance_handle_t** instance_handle)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vchi_initialise)>("vchi_initialise");
    return func(instance_handle);
}

int32_t vchi_connect(
    vchi_connection_t** connections, const uint32_t num_connections, opaque_vchi_instance_handle_t* instance_handle)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vchi_connect)>("vchi_connect");
    return func(connections, num_connections, instance_handle);
}

int32_t vchi_disconnect(opaque_vchi_instance_handle_t* instance_handle)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vchi_disconnect)>("vchi_disconnect");
    return func(instance_handle);
}

void vc_vchi_gencmd_init(
    opaque_vchi_instance_handle_t* initialise_instance, vchi_connection_t** connections, uint32_t num_connections)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vc_vchi_gencmd_init)>("vc_vchi_gencmd_init");
    return func(initialise_instance, connections, num_connections);
}

void vc_gencmd_stop(void)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vc_gencmd_stop)>("vc_gencmd_stop");
    return func();
}

int vc_gencmd(char* response, int maxlen, const char* format)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vc_gencmd)>("vc_gencmd");
    return func(response, maxlen, format);
}

int32_t vc_gpuserv_init(void)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vc_gpuserv_init)>("vc_gpuserv_init");
    return func();
}

void vc_gpuserv_deinit(void)
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vc_gpuserv_deinit)>("vc_gpuserv_deinit");
    return func();
}

int32_t vc_gpuserv_execute_code(int num_jobs, struct gpu_job_s jobs[])
{
    static auto func = resolveBcmHostLibrarySymbol<decltype(vc_gpuserv_execute_code)>("vc_gpuserv_execute_code");
    return func(num_jobs, jobs);
}
