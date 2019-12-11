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

static std::string readFile(const std::string& fileName)
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
