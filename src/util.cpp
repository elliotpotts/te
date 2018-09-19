#include <te/util.hpp>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>

std::string te::file_contents(std::string filename) {
    std::ifstream file_in {filename, std::ios_base::in};
    std::ostringstream sstr;
    std::copy (
        std::istreambuf_iterator<char>(file_in.rdbuf()),
        std::istreambuf_iterator<char>(),
        std::ostreambuf_iterator<char>(sstr)
    );
    return sstr.str();
}
