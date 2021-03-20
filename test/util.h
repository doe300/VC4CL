/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TEST_UTIL_H_
#define TEST_UTIL_H_

#include <fstream>
#include <sstream>
#include <string>

static constexpr auto hello_world_vector_src = R"(
__kernel __attribute__((reqd_work_group_size(1,1,1))) void hello_world(__global const char16* in, __global char16* out)
{
	size_t id = get_global_id(0);
	out[id] = in[id];
})";

inline std::string readFile(const std::string& fileName)
{
    std::string line;
    std::ifstream f(fileName);
    if(!f)
        throw std::invalid_argument{"Input file could not be opened: " + fileName};
    std::ostringstream s;
    do
    {
        std::getline(f, line);
        s << line << std::endl;
    } while(f.good());

    return s.str();
}

#endif /* TEST_UTIL_H_ */
