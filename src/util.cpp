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

std::size_t std::hash<glm::vec2>::operator()(glm::vec2 xy) const {
    std::size_t hash_x = std::hash<float>{}(xy.x);
    std::size_t hash_y = std::hash<float>{}(xy.y);
    return hash_x ^ (hash_y << 1);
};

std::size_t std::hash<glm::ivec2>::operator()(glm::vec2 xy) const {
    std::size_t hash_x = std::hash<int>{}(xy.x);
    std::size_t hash_y = std::hash<int>{}(xy.y);
    return hash_x ^ (hash_y << 1);
}
