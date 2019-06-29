__constant uchar message2[] = "Hello World!and some more text so it cannot be lowered into register";
__kernel void hello_world(__global uchar* out)
{
    size_t gid = get_global_id(0);
    // every kernel writes 1 single character
    out[gid] = message2[gid];
}