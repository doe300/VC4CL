
__kernel void test_sfu(__global const float16* in, __global float16* out)
{
	float16 val = *in;
	out[0] = native_recip(val);
	out[1] = native_rsqrt(val);
	out[2] = native_exp2(val);
	out[3] = native_log2(val);
}
