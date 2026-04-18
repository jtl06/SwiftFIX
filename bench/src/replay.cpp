#include "replay.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace swiftfix::bench {

std::vector<CorpusMessage> load_corpus(const std::filesystem::path& dir) {
    std::vector<CorpusMessage> out;
    if (!std::filesystem::is_directory(dir)) {
        std::ostringstream err;
        err << "corpus directory not found: " << dir;
        throw std::runtime_error(err.str());
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& path : files) {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream buf;
        buf << in.rdbuf();
        out.push_back(CorpusMessage{path.filename().string(), buf.str()});
    }
    return out;
}

std::string load_stream(const std::filesystem::path& file) {
    if (!std::filesystem::is_regular_file(file)) {
        std::ostringstream err;
        err << "stream file not found: " << file;
        throw std::runtime_error(err.str());
    }
    std::ifstream in(file, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

}  // namespace swiftfix::bench
