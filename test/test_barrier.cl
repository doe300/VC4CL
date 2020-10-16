#define TPB 8

__kernel void test_barrier(__global int* status)
{
	__global int* base = status + (get_global_id(0) + get_global_id(1) * get_global_size(0)) * (TPB + 4);
	base[0] = 0;
	barrier(CLK_LOCAL_MEM_FENCE);
	base[1] = 1;
	barrier(CLK_GLOBAL_MEM_FENCE);
	base[2] = 2;

	for(int i = 1; i < TPB; i += 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		base[3 + i] = 3 + i;
	}
}
