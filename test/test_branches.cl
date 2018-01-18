/*
 * Tests handling of branches
 */
__kernel void test_branches(const __global int* in, __global int* out)
{
	int tmp = *in;
	int result;
	bool test = tmp > 100;
out[2] = tmp;	
	if(tmp > 1000)
	{
		result = 1000;
out[3] = result;
	}
	else if(test)
	{
		result = 100;
out[3] = result;
	}
	else
	{
		result = 10;
out[3] = result;
	}

out[4] = result;
out[5] = tmp;

	switch(tmp)
	{
		case 1024:
			result += 10;
out[6] = result;
			break;
		case 512:
			result += 9;
out[7] = result;
			break;
		case 256:
			result += 8;
out[8] = result;
			break;
		case 64:
			result += 6;
out[9] = result;
			break;
		case 32:
			result += 5;
out[10] = result;
			break;
		default:
			result += 1;
out[11] = result;
			break;
	}

	out[0] = result;

	while(result < 1024)
	{
		result *= 2;
		result += 7;
	}
	out[1] = result;
}
