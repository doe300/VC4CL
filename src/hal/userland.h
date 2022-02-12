/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_USERLAND_INCLUDES
#define VC4CL_USERLAND_INCLUDES

#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

    ////
    // BCM host library
    // see "/opt/vc/include/bcm_host.h" (or "/usr/include/bcm_host.h")
    ////

    void bcm_host_init(void);
    void bcm_host_deinit(void);
    unsigned bcm_host_get_peripheral_address(void);

#define BCM_HOST_BOARD_TYPE_MODELA 0
#define BCM_HOST_BOARD_TYPE_MODELB 1
#define BCM_HOST_BOARD_TYPE_MODELAPLUS 2
#define BCM_HOST_BOARD_TYPE_MODELBPLUS 3
#define BCM_HOST_BOARD_TYPE_PI2MODELB 4
#define BCM_HOST_BOARD_TYPE_ALPHA 5
#define BCM_HOST_BOARD_TYPE_CM 6
#define BCM_HOST_BOARD_TYPE_CM2 7
#define BCM_HOST_BOARD_TYPE_PI3MODELB 8
#define BCM_HOST_BOARD_TYPE_PI0 9
#define BCM_HOST_BOARD_TYPE_CM3 0xa
#define BCM_HOST_BOARD_TYPE_CUSTOM 0xb
#define BCM_HOST_BOARD_TYPE_PI0W 0xc
#define BCM_HOST_BOARD_TYPE_PI3MODELBPLUS 0xd
#define BCM_HOST_BOARD_TYPE_PI3MODELAPLUS 0xe
#define BCM_HOST_BOARD_TYPE_FPGA 0xf
#define BCM_HOST_BOARD_TYPE_CM3PLUS 0x10
#define BCM_HOST_BOARD_TYPE_PI4MODELB 0x11
#define BCM_HOST_BOARD_TYPE_PI400 0x13
#define BCM_HOST_BOARD_TYPE_CM4 0x14
    int bcm_host_get_model_type(void);

#define BCM_HOST_PROCESSOR_BCM2835 0
#define BCM_HOST_PROCESSOR_BCM2836 1
#define BCM_HOST_PROCESSOR_BCM2837 2
#define BCM_HOST_PROCESSOR_BCM2838 3
    int bcm_host_get_processor_id(void);

    ////
    // VideoCore Host Interface general functions
    // see "/opt/vc/include/interface/vchi/vchi.h" (or "/usr/include/interface/vchi/vchi.h")
    ////

    struct opaque_vchi_instance_handle_t;
    struct vchi_connection_t;

    int32_t vchi_initialise(opaque_vchi_instance_handle_t** instance_handle);
    int32_t vchi_connect(vchi_connection_t** connections, const uint32_t num_connections,
        opaque_vchi_instance_handle_t* instance_handle);
    int32_t vchi_disconnect(opaque_vchi_instance_handle_t* instance_handle);

    ////
    // VideoCore Host Interface general command interface
    // see "/opt/vc/include/interface/vmcs_host/vc_vchi_gencmd.h" (or
    // "/usr/include/interface/vmcs_host/vc_vchi_gencmd.h")
    ////

    void vc_vchi_gencmd_init(
        opaque_vchi_instance_handle_t* initialise_instance, vchi_connection_t** connections, uint32_t num_connections);
    void vc_gencmd_stop(void);
    // Original is int vc_gencmd(char* response, int maxlen, const char* format, ...) but we do not need the additional
    // arguments for now
    int vc_gencmd(char* response, int maxlen, const char* format);

    ////
    // VideoCore Host Interface GPUS service
    // see "/opt/vc/include/interface/vmcs_host/vc_vchi_gpuserv.h" (or
    // "/usr/include/interface/vmcs_host/vc_vchi_gpuserv.h")
    ////

    // these go in command word of gpu_job_s
    // EXECUTE_VPU and EXECUTE_QPU are valid from host
    enum
    {
        EXECUTE_NONE,
        EXECUTE_VPU,
        EXECUTE_QPU,
        EXECUTE_SYNC,
        LAUNCH_VPU1
    };

    struct qpu_job_s
    {
        // parameters for qpu job
        uint32_t jobs;
        uint32_t noflush;
        uint32_t timeout;
        uint32_t dummy;
        uint32_t control[12][2];
    };

    struct sync_job_s
    {
        // parameters for syncjob
        // bit 0 set means wait for preceding vpu jobs to complete
        // bit 1 set means wait for preceding qpu jobs to complete
        uint32_t mask;
        uint32_t dummy[27];
    };

    struct gpu_callback_s
    {
        // callback to call when complete (can be NULL)
        void (*func)();
        void* cookie;
    };

    struct gpu_internal_s
    {
        void* message;
        int refcount;
    };

    struct gpu_job_s
    {
        // from enum above
        uint32_t command;
        // qpu or vpu jobs
        union
        {
            // struct vpu_job_s v; // not of interest to us
            struct qpu_job_s q;
            struct sync_job_s s;
        } u;
        // callback function to call when complete
        struct gpu_callback_s callback;
        // for internal use - leave as zero
        struct gpu_internal_s internal;
    };

    int32_t vc_gpuserv_init(void);
    void vc_gpuserv_deinit(void);

    int32_t vc_gpuserv_execute_code(int num_jobs, struct gpu_job_s jobs[]);

    ////
    // VideoCore Shared Memory user-space library
    // see "/opt/vc/include/interface/vcsm/user-vcsm.h" (or "/usr/include/interface/vcsm/user-vcsm.h")
    ////

    typedef enum
    {
        VCSM_CACHE_TYPE_NONE = 0,     // No caching applies.
        VCSM_CACHE_TYPE_HOST,         // Allocation is cached on host (user space).
        VCSM_CACHE_TYPE_VC,           // Allocation is cached on videocore.
        VCSM_CACHE_TYPE_HOST_AND_VC,  // Allocation is cached on both host and videocore.
        VCSM_CACHE_TYPE_PINNED = 0x80 // Pre-pin (vs. allocation on first access), see
                                      // https://github.com/xbmc/xbmc/commit/3d1e2d73dbbcbc8c1e6bf63f7e563ae96312c394
    } VCSM_CACHE_TYPE_T;

    int vcsm_init_ex(int want_cma, int fd);
    void vcsm_exit(void);

    unsigned int vcsm_malloc_cache(unsigned int size, VCSM_CACHE_TYPE_T cache, const char* name);
    void vcsm_free(unsigned int handle);

    unsigned int vcsm_vc_addr_from_hdl(unsigned int handle);

    void* vcsm_lock(unsigned int handle);
    int vcsm_unlock_ptr(void* usr_ptr);

    int vcsm_export_dmabuf(unsigned int vcsm_handle);

    struct vcsm_user_clean_invalid2_s
    {
        unsigned char op_count;
        unsigned char zero[3];
        struct vcsm_user_clean_invalid2_block_s
        {
            unsigned short invalidate_mode;
            unsigned short block_count;
            void* start_address;
            unsigned int block_size;
            unsigned int inter_block_stride;
        } s[0];
    };

    int vcsm_clean_invalid2(struct vcsm_user_clean_invalid2_s* s);

// Taken from kernel header "linux/drivers/staging/vc04_services/include/linux/broadcom/vc_sm_cma_ioctl.h "
/*
 * Cache functions to be set to struct vc_sm_cma_ioctl_clean_invalid2 invalidate_mode.
 */
#define VC_SM_CACHE_OP_NOP 0x00
#define VC_SM_CACHE_OP_INV 0x01
#define VC_SM_CACHE_OP_CLEAN 0x02
#define VC_SM_CACHE_OP_FLUSH 0x03

#ifdef __cplusplus
}
#endif

#endif /* VC4CL_USERLAND_INCLUDES */
