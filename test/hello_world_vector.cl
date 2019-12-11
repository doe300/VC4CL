__kernel 
__attribute__((reqd_work_group_size(1,1,1)))
void hello_world(__global const char16* in, __global char16* out)
{
	size_t id = get_global_id(0);
	out[id] = in[id];
}
