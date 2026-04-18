// replay.hpp — load a corpus directory into memory so benchmarks aren't
// contaminated by filesystem I/O.
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace swiftfix::bench {

struct CorpusMessage {
    std::string name;     // source filename, for reports
    std::string bytes;    // raw FIX frame bytes
};

// Load every regular file under `dir` as one corpus message. Files are
// read in lexicographic order so runs are reproducible.
std::vector<CorpusMessage> load_corpus(const std::filesystem::path& dir);

// Slurp a single concatenated-frames file (as produced by
// corpus/generate.py --bulk) into memory. The returned string is the
// raw byte stream — FIX::Parser splits it into messages itself.
std::string load_stream(const std::filesystem::path& file);

}  // namespace swiftfix::bench
