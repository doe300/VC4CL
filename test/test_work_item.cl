
__kernel void test_work_item(__global uint* ptr)
{
	__global uint* out = ptr + mul24(get_global_id(0), (uint)24);
	out[0] = get_work_dim();
	out[1] = get_global_size(0);
	out[2] = get_global_size(1);
	out[3] = get_global_size(2);
	out[4] = get_global_id(0);
	out[5] = get_global_id(1);
	out[6] = get_global_id(2);
	out[7] = get_global_offset(0);
	out[8] = get_global_offset(1);
	out[9] = get_global_offset(2);
	out[10] = get_num_groups(0);
	out[11] = get_num_groups(1);
	out[12] = get_num_groups(2);
	out[13] = get_group_id(0);
	out[14] = get_group_id(1);
	out[15] = get_group_id(2);
	out[16] = get_local_size(0);
	out[17] = get_local_size(1);
	out[18] = get_local_size(2);
	out[19] = get_local_id(0);
	out[20] = get_local_id(1);
	out[21] = get_local_id(2);
	out[22] += 1;
}
