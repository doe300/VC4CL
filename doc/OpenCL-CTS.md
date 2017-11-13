# Tests

### Computeinfo (computeinfo/computeinfo)
PASSED

### Basic (basic/test_basic)
Run with work-size of 8

| Test name                           | Status | Reason |
|-------------------------------------|--------|--------|
| hostptr                             | PASSED ||
| fpmath_float                        | PASSED ||
| fpmath_float2                       | PASSED ||
| fpmath_float4                       | PASSED ||
| intmath_int                         | PASSED ||
| intmath_int2                        | PASSED ||
| intmath_int4                        | PASSED ||
| intmath_long                        | skipped ||
| intmath_long2                       | skipped ||
| intmath_long4                       | skipped ||
| hiloeo                              | PASSED/FAILED | fails in trying to compile long-tests |
| if                                  | PASSED ||
| sizeof                              | PASSED | *skipped image, sampler* |
| loop                                | PASSED ||
| pointer_cast                        | PASSED ||
| local_arg_def                       | PASSED ||
| local_kernel_def                    | PASSED ||
| local_kernel_scope                  | FAILED | local work-size exceeds maximum |
| constant                            | FAILED ||
| constant_source                     | FAILED | value mismatch |
| readimage                           | skipped ||
| readimage_int16                     | skipped ||
| readimage_fp32                      | skipped ||
| writeimage                          | skipped ||
| writeimage_int16                    | skipped ||
| writeimage_fp32                     | skipped ||
| mri_one                             | skipped ||
| mri_multiple                        | skipped ||
| image_r8                            | skipped ||
| barrier                             | FAILED ||
| int2float                           | PASSED ||
| float2int                           | PASSED ||
| imagereadwrite                      | skipped ||
| imagereadwrite3d                    | skipped ||
| readimage3d                         | skipped ||
| readimage3d_int16                   | skipped ||
| readimage3d_fp32                    | skipped ||
| bufferreadwriterect                 | FAILED | value mismatch |
| arrayreadwrite                      | PASSED |
| arraycopy                           | FAILED | fails for CL_MEM_USE_HOST_PTR |
| imagearraycopy                      | skipped ||
| imagearraycopy3d                    | skipped ||
| imagecopy                           | skipped ||
| imagecopy3d                         | skipped ||
| imagerandomcopy                     | skipped ||
| arrayimagecopy                      | skipped ||
| arrayimagecopy3d                    | skipped ||
| imagenpot                           | skipped ||
| vload_global                        | PASSED ||
| vload_local                         | FAILED | result value mismatch |
| vload_constant                      | PASSED ||
| vload_private                       | FAILED | std::bad_alloc |
| vstore_global                       | FAILED | result value mismatch (only gentype3 tests) |
| vstore_local                        | FAILED | result value mismatch (only gentype2 tests, alignment?) |
| vstore_private                      | FAILED | std::bad_alloc |
| createkernelsinprogram              | PASSED ||
| imagedim_pow2                       | skipped ||
| imagedim_non_pow2                   | skipped ||
| image_param                         | skipped ||
| image_multipass_integer_coord       | skipped ||
| image_multipass_float_coord         | skipped ||
| explicit_s2v_bool                   | skipped ||
| explicit_s2v_char                   | PASSED ||
| explicit_s2v_uchar                  | PASSED ||
| explicit_s2v_short                  | PASSED ||
| explicit_s2v_ushort                 | PASSED ||
| explicit_s2v_int                    | PASSED ||
| explicit_s2v_uint                   | PASSED ||
| explicit_s2v_long                   | skipped ||
| explicit_s2v_ulong                  | skipped ||
| explicit_s2v_float                  | PASSED ||
| explicit_s2v_double                 | skipped ||
| enqueue_map_buffer                  | PASSED ||
| enqueue_map_image                   | skipped ||
| work_item_functions                 | FAILED | return value mismatch ("get_global_size(0) did not return proper value for 1 dimensions (expected 11, got 0)"), compilation error |
| astype                              | PASSED ||
| async_copy_global_to_local          | FAILED | compilation error |
| async_copy_local_to_global          | FAILED | compilation error |
| async_strided_copy_global_to_local  | FAILED | compilation error |
| async_strided_copy_local_to_global  | FAILED | compilation error |
| prefetch                            | FAILED | result value mismatch (got unexpected 0, all vector-types) |
| kernel_call_kernel_function         | FAILED | result value mismatch |
| host_numeric_constants              | PASSED ||
| kernel_numeric_constants            | FAILED | "GPUs are required to support images in OpenCL 1.1 and later." |
| kernel_limit_constants              | PASSED | *Skipping INFINITY and NAN tests on embedded device (INF/NAN not supported on this device)* | 
| kernel_preprocessor_macros          | PASSED ||
| parameter_types                     | PASSED ||
| vector_creation                     | PASSED | *8-element vectors take a lot longer than 1,2 ,3 or 4 to compile (both optimizer and code-generation)* |
| vec_type_hint                       | PASSED ||
| kernel_memory_alignment_local       | PASSED ||
| kernel_memory_alignment_global      | PASSED ||
| kernel_memory_alignment_constant    | PASSED ||
| kernel_memory_alignment_private     | exception | bad_alloc |
| global_work_offsets                 | FAILED | result value mismatch, hangs |
| get_global_offset                   | PASSED ||


### API (api/test_api)
Run with work-size of 8

| Test name                           | Status | Reason |
|-------------------------------------|--------|--------|
| get_platform_info                   | PASSED ||
| get_sampler_info                    | skipped ||
| get_command_queue_info              | PASSED ||
| get_context_info                    | PASSED ||
| get_device_info                     | PASSED ||
| enqueue_task                        | PASSED ||
| binary_get                          | PASSED ||
| binary_create                       | FAILED | binaries do not match (kernel-info!) |
| kernel_required_group_size          | PASSED ||
| release_kernel_order                | PASSED ||
| release_during_execute              | PASSED ||
| load_single_kernel                  | PASSED ||
| load_two_kernels                    | PASSED ||
| load_two_kernels_in_one             | PASSED ||
| load_two_kernels_manually           | PASSED ||
| get_program_info_kernel_names       | PASSED ||
| get_kernel_arg_info                 | FAILED | "Unable to get program info num kernels!", "Unable to get argument address qualifier!" |
| create_kernels_in_program           | PASSED ||
| get_kernel_info                     | PASSED ||
| execute_kernel_local_sizes          | FAILED | value mismatch |
| set_kernel_arg_by_index             | PASSED |
| set_kernel_arg_constant             | PASSED ||
| set_kernel_arg_struct_array         | PASSED |
| kernel_global_constant              | PASSED |
| min_max_thread_dimensions           | PASSED |
| min_max_work_items_sizes            | PASSED ||
| min_max_work_group_size             | PASSED ||
| min_max_read_image_args             | skipped ||
| min_max_write_image_args            | skipped ||
| min_max_mem_alloc_size              | PASSED ||
| min_max_image_2d_width              | skipped ||
| min_max_image_2d_height             | skipped ||
| min_max_image_3d_width              | skipped ||
| min_max_image_3d_height             | skipped ||
| min_max_image_3d_depth              | skipped ||
| min_max_image_array_size            | skipped ||
| min_max_image_buffer_size           | skipped ||
| min_max_parameter_size              | PASSED ||
| min_max_samplers                    | skipped ||
| min_max_constant_buffer_size        | FAILED | CL_OUT_OF_RESOURCES |
| min_max_constant_args               | FAILED | compilation error (probably register-allocation, since kernel has 64 args) |
| min_max_compute_units               | PASSED ||
| min_max_address_bits                | PASSED ||
| min_max_single_fp_config            | PASSED ||
| min_max_double_fp_config            | PASSED ||
| min_max_local_mem_size              | FAILED | CL_OUT_OF_RESOURCES | 
| min_max_kernel_preferred_work_group_size_multiple | PASSED ||
| min_max_execution_capabilities      | PASSED ||
| min_max_queue_properties            | PASSED ||
| min_max_device_version              | PASSED ||
| min_max_language_version            | PASSED ||
| kernel_arg_changes                  | PASSED ||
| kernel_arg_multi_setup_random       | FAILED | value mismatch |
| native_kernel                       | PASSED ||
| create_context_from_type            | PASSED |
| platform_extensions                 | PASSED ||
| get_platform_ids                    | FAILED | CL_CONTEXT_PROPERTIES invalid value |
| bool_type                           | PASSED ||
| repeated_setup_cleanup              | FAILED | result value mismatch |
| retain_queue_single                 | PASSED ||
| retain_queue_multiple               | PASSED ||
| retain_mem_object_single            | PASSED ||
| retain_mem_object_multiple          | PASSED ||
| min_data_type_align_size_alignment  | PASSED ||
| mem_object_destructor_callback      | FAILED | wrong order |
| null_buffer_arg                     | FAILED | compilation error |
| get_buffer_info                     | FAILED | invalid mem object size |
| get_image2d_info                    | skipped ||
| get_image3d_info                    | skipped ||
| get_image1d_info                    | skipped ||
| get_image1d_array_info              | skipped ||
| get_image2d_array_info              | skipped ||


### Compiler (compiler/test_compiler)

| Test name                                              | Status | Reason |
|--------------------------------------------------------|--------|--------|
| load_program_source                                    | PASSED ||
| load_multistring_source                                | PASSED ||
| load_two_kernel_source                                 | PASSED ||
| load_null_terminated_source                            | PASSED ||
| load_null_terminated_multi_line_source                 | PASSED ||
| load_null_terminated_partial_multi_line_source         | PASSED ||
| load_discreet_length_source                            | PASSED ||
| get_program_source                                     | PASSED ||
| get_program_build_info                                 | PASSED ||
| get_program_info                                       | PASSED ||
| large_compile                                          | PASSED ||
| async_build                                            | PASSED | *not really run asynchronously* |
| options_build_optimizations                            | FAILED | -cl-finite-math-only not in PCH |
| options_build_macro                                    | PASSED ||
| options_build_macro_existence                          | PASSED ||
| options_include_directory                              | FAILED | include path not found |
| options_denorm_cache                                   | PASSED ||
| preprocessor_define_udef                               | PASSED ||
| preprocessor_include                                   | FAILED | include path not found |
| preprocessor_line_error                                | FAILED | wrong status returned? |
| preprocessor_pragma                                    | FAILED ||
| compiler_defines_for_extensions                        | FAILED | cl_khr_il_program not approved by Khronos?!? |
| image_macro                                            | FAILED ||
| simple_compile_only                                    | PASSED ||
| simple_static_compile_only                             | PASSED ||
| simple_extern_compile_only                             | PASSED ||
| simple_compile_with_callback                           | PASSED ||
| simple_embedded_header_compile                         | FAILED | include path (embedded) not found |
| simple_link_only                                       | FAILED ||
| two_file_regular_variable_access
| two_file_regular_struct_access
| two_file_regular_function_access
| simple_link_with_callback                              | PASSED ||
| simple_embedded_header_link                            | FAILED ||
| execute_after_simple_compile_and_link                  | FAILED | result value mismatch |
| execute_after_simple_compile_and_link_no_device_info
| execute_after_simple_compile_and_link_with_defines
| execute_after_simple_compile_and_link_with_callbacks
| execute_after_simple_library_with_link
| execute_after_two_file_link
| execute_after_embedded_header_link                     | FAILED | Unable to compile a simple program with embedded header! (CL_COMPILER_NOT_AVAILABLE) |
| execute_after_included_header_link                     | FAILED | "Unable to create directory foo!" |
| execute_after_serialize_reload_object                  | FAILED | Unable to set the first kernel argument! (CL_INVALID_ARG_VALUE) |
| execute_after_serialize_reload_library
| simple_library_only
| simple_library_with_callback
| simple_library_with_link
| two_file_link
| multi_file_libraries
| multiple_files
| multiple_libraries
| multiple_files_multiple_libraries
| multiple_embedded_headers                              | FAILED | Unable to compile a simple program! (CL_COMPILER_NOT_AVAILABLE) |
| program_binary_type                                    | FAILED | compilation error |
| compile_and_link_status_options_log                    | FAILED | Unable to compile a simple program! (CL_COMPILER_NOT_AVAILABLE) |


### Common Functions (commonfns/test_commonfns)
Run with work-size of 8

| Test name       | Status | Reason |
|-----------------|--------|--------|
| clamp           | PASSED ||
| degrees         | PASSED ||
| fmax            | PASSED ||
| fmaxf           | PASSED ||
| fmin            | PASSED ||
| fminf           | PASSED ||
| max             | PASSED ||
| maxf            | PASSED ||
| min             | PASSED ||
| minf            | FAILED | result value mismatch (min(0x1.8fe16p+25, 0x1.7c4aap+26) = *0x1.8fe16p+25 vs 0x1.7c4aap+26) |
| mix             | PASSED ||
| radians         | PASSED ||
| step            | PASSED ||
| stepf           | PASSED ||
| smoothstep      | PASSED ||
| smoothstepf     | PASSED ||
| sign            | PASSED ||


### Geometric Functions (geometrics/test_geometrics)
Run with work-size of 8

| Test name             | Status | Reason |
|-----------------------|--------|--------|
| geom_cross            | FAILED | result value mismatch, got 0 where not expected (vloadn?) |
| geom_dot              | FAILED | result value mismatch, single failing test-case in dot3 |
| geom_distance         | FAILED | value mismatch, got -inf where not expected (sqrt?) |
| geom_fast_distance    | FAILED | value mismatch, single failing test-case in fast_distance3, failed to create vector B for 4-element test |
| geom_length           | FAILED | value mismatch, got -inf where not expected |
| geom_fast_length      | FAILED | value mismatch, single failing test-case in fast_length3 (ULP?? native_sqrt to inaccurate?), failed to create vector B for 4-element test |
| geom_normalize        | FAILED | value mismatch for 1.9p-145 -> denormal value? |
| geom_fast_normalize   | PASSED ||

 
### Relationals (relationals/test_relationals)
Run with work-size of 8

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| relational_any                  | FAILED | result value mismatch (for all types) |
| relational_all                  | FAILED | result value mismatch (for all types with less than 16 elements) |
| relational_bitselect            | FAILED | value mismatch |
| relational_select_signed        | FAILED | value mismatch (for all types) |
| relational_select_unsigned      | FAILED | value mismatch (for all types) |
| relational_isequal              | FAILED | fails for NaN (does not recognize as equal) |
| relational_isnotequal           | FAILED | fails for NaN (does not recognize as equal) |
| relational_isgreater            | FAILED | fails for comparisons with NaN |
| relational_isgreaterequal       | FAILED | fails for comparisons with NaN |
| relational_isless
| relational_islessequal
| relational_islessgreater
| shuffle_copy                    | PASSED ||
| shuffle_function_call           | PASSED ||
| shuffle_array_cast              | PASSED ||
| shuffle_built_in                | PASSED ||
| shuffle_built_in_dual_input     | FAILED | value mismatch (only short8 -> short2, int8 -> int2, int16 -> int2, uint8 -> uint2), compilation error (uint2 -> uint4) |

### Thread Dimensions (thread_dimensions/test_thread_dimensions)
Run with work-size of 8

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| quick_1d_explicit_local         | FAILED | error in CTS source, identifier final_x_size is split up (final_x_    size) |
| quick_2d_explicit_local
| quick_3d_explicit_local
| quick_1d_implicit_local
| quick_2d_implicit_local
| quick_3d_implicit_local
| full_1d_explicit_local
| full_2d_explicit_local
| full_3d_explicit_local
| full_1d_implicit_local
| full_2d_implicit_local
| full_3d_implicit_local
		
		
### Atomics (atomics/test_atomics)

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| atomic_add                      | FAILED | value mismatch |
| atomic_sub                      | FAILED | value mismatch |
| atomic_xchg                     | PASSED ||
| atomic_min                      | FAILED | value mismatch |
| atomic_max
| atomic_inc                      | PASSED ||
| atomic_dec                      | PASSED ||
| atomic_cmpxchg                  | PASSED ||
| atomic_and                      | FAILED | compilation error (long constant) |
| atomic_or                       | FAILED | compilation error |
| atomic_xor
| atomic_add_index                | FAILED | "wrong number of instances" |
| atomic_add_index_bin            | FAILED | "FAILED to set kernel arguments: CL_INVALID_ARG_VALUE" |

### Profiling (profiling/test_profiling)
PASSES all for 8 work-items

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| read_array_int                  | PASSED ||
| read_array_uint                 | PASSED ||
| read_array_long                 | skipped ||
| read_array_ulong                | skipped ||
| read_array_short                | PASSED ||
| read_array_ushort               | PASSED ||
| read_array_float                | PASSED ||
| read_array_char                 | PASSED ||
| read_array_uchar                | PASSED ||
| read_array_struct               | PASSED ||
| write_array_int                 | PASSED ||
| write_array_uint                | PASSED ||
| write_array_long                | skipped ||
| write_array_ulong               | skipped ||
| write_array_short               | PASSED ||
| write_array_ushort              | PASSED ||
| write_array_float               | PASSED ||
| write_array_char                | PASSED ||
| write_array_uchar               | PASSED ||
| write_array_struct              | PASSED ||
| read_image_float                | skipped ||
| read_image_int                  | skipped ||
| read_image_uint                 | skipped ||
| write_image_float               | skipped ||
| write_image_char                | skipped ||
| write_image_uchar               | skipped ||
| copy_array                      | PASSED ||
| copy_partial_array              | PASSED/FAILED | copy size exceeds buffer size (for size < 8) | 
| copy_image                      | skipped ||
| copy_array_to_image             | skipped ||
| execute                         | skipped ||

### Events (events/test_events)

| Test name                                                          | Status | Reason |
|--------------------------------------------------------------------|--------|--------|
| event_get_execute_status                                           | PASSED ||
| event_get_write_array_status                                       | PASSED ||
| event_get_read_array_status                                        | PASSED ||
| event_get_info                                                     | PASSED ||
| event_wait_for_execute                                             | PASSED ||
| event_wait_for_array                                               | PASSED ||
| event_flush                                                        | PASSED ||
| event_finish_execute                                               | PASSED ||
| event_finish_array                                                 | FAILED | "Incorrect status returned from clGetErrorStatus after array write complete (1:CL_RUNNING)" |
| event_release_before_done                                          | PASSED ||
| event_enqueue_marker                                               | PASSED ||
| event_enqueue_marker_with_event_list                               | PASSED ||
| event_enqueue_barrier_with_event_list                              | PASSED ||
| out_of_order_event_waitlist_single_queue                           | skipped ||
| out_of_order_event_waitlist_multi_queue                            | skipped ||
| out_of_order_event_waitlist_multi_queue_multi_device               | skipped ||
| out_of_order_event_enqueue_wait_for_events_single_queue            | skipped ||
| out_of_order_event_enqueue_wait_for_events_multi_queue             | skipped ||
| out_of_order_event_enqueue_wait_for_events_multi_queue_multi_device| skipped ||
| out_of_order_event_enqueue_marker_single_queue                     | skipped ||
| out_of_order_event_enqueue_marker_multi_queue                      | skipped ||
| out_of_order_event_enqueue_marker_multi_queue_multi_device         | skipped ||
| out_of_order_event_enqueue_barrier_single_queue                    | skipped ||
| waitlists                                                          | skipped ||
| test_userevents                                                    | FAILED | "clGetEventInfo 0 returned wrong status before user event" |
| callbacks                                                          | segfault | error -14 in wait list |
| callbacks_simultaneous                                             | FAILED | error -14 in wait-list |
| userevents_multithreaded                                           | FAILED | "Unable to create user gate event!" |

### Allocations (allocations/test_allocations)

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| single 5 all                    | FAILED | allocation failed |
| multiple 5 all


### Printf (printf/test_printf)
Not supported 

### Buffers (buffers/test_buffers)
Run with work-size of 8
TODO: All buffer_map_* tests fail for CL_MEM_USE_HOST_PTR

| Test name                               | Status | Reason |
|-----------------------------------------|--------|--------|
| buffer_read_async_int                   | PASSED ||
| buffer_read_async_uint                  | PASSED ||
| buffer_read_async_long                  | skipped ||
| buffer_read_async_ulong                 | skipped ||
| buffer_read_async_short                 | PASSED || 
| buffer_read_async_ushort                | PASSED ||
| buffer_read_async_char                  | PASSED ||
| buffer_read_async_uchar                 | PASSED ||
| buffer_read_async_float                 | PASSED ||
| buffer_read_array_barrier_int           | PASSED ||
| buffer_read_array_barrier_uint          | PASSED ||
| buffer_read_array_barrier_long          | skipped ||
| buffer_read_array_barrier_ulong         | skipped ||
| buffer_read_array_barrier_short         | PASSED ||
| buffer_read_array_barrier_ushort        | PASSED ||
| buffer_read_array_barrier_char          | PASSED ||
| buffer_read_array_barrier_uchar         | PASSED ||
| buffer_read_array_barrier_float         | PASSED ||
| buffer_read_int                         | PASSED ||
| buffer_read_uint                        | PASSED ||
| buffer_read_long                        | skipped ||
| buffer_read_ulong                       | skipped ||
| buffer_read_short                       | PASSED ||
| buffer_read_ushort                      | PASSED ||
| buffer_read_float                       | PASSED ||
| buffer_read_half                        | not implemented ||
| buffer_read_char                        | PASSED ||
| buffer_read_uchar                       | PASSED ||
| buffer_read_struct                      | PASSED ||
| buffer_read_random_size                 | PASSED |
| buffer_map_read_int                     | PASSED/FAILED ||
| buffer_map_read_uint                    | PASSED/FAILED ||
| buffer_map_read_long                    | skipped ||
| buffer_map_read_ulong                   | skipped ||
| buffer_map_read_short                   | FAILED | fails for tests with CL_MEM_USE_HOST_PTR |
| buffer_map_read_ushort                  | FAILED ||
| buffer_map_read_char                    | FAILED ||
| buffer_map_read_uchar                   | FAILED ||
| buffer_map_read_float                   | PASSED ||
| buffer_map_read_struct                  | PASSED/FAILED | fails for tests with CL_MEM_USE_HOST_PTR |
| buffer_map_write_int                    | PASSED ||
| buffer_map_write_uint                   | PASSED ||
| buffer_map_write_long                   | skipped ||
| buffer_map_write_ulong                  | skipped ||
| buffer_map_write_short                  | PASSED ||
| buffer_map_write_ushort                 | PASSED ||
| buffer_map_write_char                   | PASSED ||
| buffer_map_write_uchar                  | PASSED ||
| buffer_map_write_float                  | PASSED ||
| buffer_map_write_struct                 | PASSED ||
| buffer_write_int                        | FAILED | fails for tests with src CL_MEM_USE_HOST_PTR | 
| buffer_write_uint
| buffer_write_short
| buffer_write_ushort
| buffer_write_char
| buffer_write_uchar
| buffer_write_float
| buffer_write_half
| buffer_write_long                       | skipped ||
| buffer_write_ulong                      | skipped || 
| buffer_write_struct                     | FAILED | fails for tests with src CL_MEM_USE_HOST_PTR | 
| buffer_write_async_int                  | PASSED ||
| buffer_write_async_uint                 | PASSED ||
| buffer_write_async_short                | PASSED ||
| buffer_write_async_ushort               | PASSED ||
| buffer_write_async_char                 | PASSED ||
| buffer_write_async_uchar                | PASSED ||
| buffer_write_async_float                | PASSED ||
| buffer_write_async_long                 | skipped ||
| buffer_write_async_ulong                | skipped ||
| buffer_copy                             | FAILED | fails for tests with src CL_MEM_USE_HOST_PTR | 
| buffer_partial_copy                     | FAILED | fails for tests with src CL_MEM_USE_HOST_PTR |
| mem_read_write_flags                    | PASSED ||
| mem_write_only_flags                    | PASSED ||
| mem_read_only_flags                     | PASSED ||
| mem_copy_host_flags                     | PASSED ||
| mem_alloc_ref_flags                     | not implemented |
| array_info_size                         | PASSED || 
| sub_buffers_read_write                  | FAILED | compilation error, "Validation failure outside of a sub-buffer! (Shouldn't be possible, but it happened" *runs VERY long* |
| sub_buffers_read_write_dual_devices     | skipped ||
| sub_buffers_overlapping                 | FAILED | validation failed |
| buffer_fill_int                         | PASSED || 
| buffer_fill_uint                        | PASSED ||
| buffer_fill_short                       | PASSED ||
| buffer_fill_ushort                      | PASSED ||
| buffer_fill_char                        | PASSED ||
| buffer_fill_uchar                       | PASSED ||
| buffer_fill_long                        | skipped ||
| buffer_fill_ulong                       | skipped ||
| buffer_fill_float                       | PASSED || 
| buffer_fill_struct                      | PASSED ||
| buffer_migrate                          | FAILED | Failed set kernel argument 1.! (CL_INVALID_ARG_VALUE) in buffers/test_buffer_migrate.c:276 |
| image_migrate                           | skipped ||

### Images (API Info) (images/clGetInfo/test_cl_get_info)
SKIPPED

### Images (Kernel Methods) (images/kernel_image_methods/test_kernel_image_methods )
SKIPPED

### Images (Kernel) (images/kernel_read_write/test_image_streams CL_FILTER_NEAREST)
SKIPPED

### Images (Kernel pitch) (images/kernel_read_write/test_image_streams use_pitches CL_FILTER_NEAREST)
SKIPPED

### Images (Kernel max size) (images/kernel_read_write/test_image_streams max_images CL_FILTER_NEAREST)
SKIPPED

### Images (clCopyImage) (images/clCopyImage/test_cl_copy_images)
SKIPPED

### Images (clCopyImage max size) (images/clCopyImage/test_cl_copy_images max_images )
SKIPPED

### Images (clReadWriteImage) (images/clReadWriteImage/test_cl_read_write_images )
SKIPPED

### Images (clReadWriteImage pitch) (images/clReadWriteImage/test_cl_read_write_images use_pitches )
SKIPPED

### Images (clReadWriteImage max size) (images/clReadWriteImage/test_cl_read_write_images max_images )
SKIPPED

### Images (clFillImage) (images/clFillImage/test_cl_fill_images )
SKIPPED

### Images (clFillImage pitch) (images/clFillImage/test_cl_fill_images use_pitches )
SKIPPED

### Images (clFillImage max size) (images/clFillImage/test_cl_fill_images max_images )
SKIPPED

### Images (Samplerless) (images/samplerlessReads/test_samplerless_reads )
SKIPPED

### Images (Samplerless pitch) (images/samplerlessReads/test_samplerless_reads use_pitches )
SKIPPED

### Images (Samplerless max size) (images/samplerlessReads/test_samplerless_reads max_images )
SKIPPED

### Mem (Host Flags) (mem_host_flags/test_mem_host_flags)
Test does not exist

### Headers (cl_typen) (headers/test_headers)
??

### Headers (cl.h standalone) (headers/test_cl_h)
PASSED

### Headers (cl_platform.h standalone) (headers/test_cl_platform_h)
PASSED

### Headers (cl_gl.h standalone) (headers/test_cl_gl_h)
not supported

### Headers (opencl.h standalone) (headers/test_opencl_h)
PASSED

### Headers (cl.h standalone C99) (headers/test_cl_h_c99)
Test does not exist

### Headers (cl_platform.h standalone C99) (headers/test_cl_platform_h_c99)
Test does not exist

### Headers (cl_gl.h standalone C99) (headers/test_cl_gl_h_c99)
not supported

### Headers (opencl.h standalone C99) (headers/test_opencl_h_c99)
Test does not exist

### OpenCL-GL Sharing (gl/test_gl)
not supported

### Select (select/test_select)
FAILED (value mismatch for every scalar type)

### Contractions (contractions/contractions)
FAILED (value mismatch: unexpected zero/nan, where nan/zero is expected)

### Math (math_brute_force/bruteforce)

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| -l (link check only)            | crashes  | corrupted doubly-linked list, preventSleep not supported |
| -w (Wimpy mode, only check few) |||

### Integer Ops (integer_ops/test_integer_ops)

| Test name                       | Status | Reason |
|---------------------------------|--------|--------|
| integer_clz                     | PASSED ||
| integer_hadd                    | FAILED | value mismatch (only for short test-cases and int3) |
| integer_rhadd                   | FAILED | value mismatch (only for short test-cases and int3) |
| integer_mul_hi                  | FAILED | value mismatch (only for char, short, int when negative number involved and uint) |
| integer_rotate                  | PASSED ||
| integer_clamp                   | FAILED | value mismatch (only short and uint test-cases) |
| integer_mad_sat                 | FAILED | value mismatch |
| integer_mad_hi                  | FAILED | value mismatch |
| integer_min                     | FAILED | value mismatch |
| integer_max                     | FAILED | value mismatch |
| integer_upsample                | FAILED | value mismatch (only for short test-cases) |
| integer_abs                     | PASSED ||
| integer_abs_diff                | FAILED | value mismatch |
| integer_add_sat                 | FAILED | value mismatch (only for short, int, uint test-cases; for int/uint only for saturation exceeded) |
| integer_sub_sat                 | FAILED | value mismatch (only for short, int, uint test-cases; for int/uint only for saturation exceeded) |
| integer_addAssign               | FAILED | value mismatch (for gentype3, test-sample 8) |
| integer_subtractAssign
| integer_multiplyAssign          | FAILED | value mismatch (for gentype3, test-sample 8) |
| integer_divideAssign            | FAILED | value mismatch (often for small numbers (0-2), actual result is negative) |
| integer_moduloAssign
| integer_andAssign               | PASSED ||
| integer_orAssign                | PASSED ||
| integer_exclusiveOrAssign       | FAILED | value mismatch (for gentype3, test-sample 8) |
| unary_ops_increment             | FAILED | value mismatch |
| unary_ops_decrement
| unary_ops_full
| integer_mul24                   | PASSED ||
| integer_mad24                   | PASSED ||
| long_math                       | skipped ||
| long_logic                      | skipped ||
| long_shift                      | skipped ||
| long_compare                    | skipped ||
| ulong_math                      | skipped ||
| ulong_logic                     | skipped ||
| ulong_shift                     | skipped ||
| ulong_compare                   | skipped ||
| int_math                        | FAILED | compilation error, validation failed, CL_OUT_OF_RESOURCES |
| int_logic
| int_shift
| int_compare
| uint_math
| uint_logic
| uint_shift
| uint_compare
| short_math
| short_logic
| short_shift
| short_compare
| ushort_math
| ushort_logic
| ushort_shift
| ushort_compare
| char_math
| char_logic
| char_shift
| char_compare
| uchar_math
| uchar_logic
| uchar_shift
| uchar_compare
| popcount                           | PASSED ||
| quick_long_math                    | skipped ||
| quick_long_logic                   | skipped ||
| quick_long_shift                   | skipped ||
| quick_long_compare                 | skipped ||
| quick_ulong_math                   | skipped ||
| quick_ulong_logic                  | skipped ||
| quick_ulong_shift                  | skipped ||
| quick_ulong_compare                | skipped ||
| quick_int_math                     | FAILED | CL_OUT_OF_RESOURCES *runs VERY long* |
| quick_int_logic                    | FAILED | result mismatch, CL_OUT_OF_RESOURCES |
| quick_int_shift                    | FAILED | result value mismatch + CL_OUT_OF_RESOURCES |
| quick_int_compare                  | FAILED | result value mismatch + CL_OUT_OF_RESOURCES |
| quick_uint_math
| quick_uint_logic
| quick_uint_shift
| quick_uint_compare
| quick_short_math
| quick_short_logic
| quick_short_shift
| quick_short_compare
| quick_ushort_math
| quick_ushort_logic
| quick_ushort_shift
| quick_ushort_compare
| quick_char_math
| quick_char_logic
| quick_char_shift
| quick_char_compare
| quick_uchar_math
| quick_uchar_logic
| quick_uchar_shift
| quick_uchar_compare               | FAILED | result mismatch (?:), CL_OUT_OF_RESOURCES (>) |
| vector_scalar                     | FAILED | result value mismatch |


### Half Ops (half/Test_half)
PASSED, but has error with not enough space to allocate
