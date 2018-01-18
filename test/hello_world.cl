
__kernel void hello_world(__global char16* out)
{
	const char16 text = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', '\0', '\0', '\0', '\0'};
	size_t id = get_global_id(0);
	out[id] = text;
}
