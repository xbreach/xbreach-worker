#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace xbreach::worker {

// Streams a (possibly compressed) text file line by line without loading it
// whole. Lines are returned without their trailing "\n" or "\r\n". Bytes are
// passed through verbatim; encoding normalization is left to the caller.
class LineReader {
  public:
    virtual ~LineReader() = default;

    // Reads the next line into `line`. Returns false at end of input.
    virtual bool next_line(std::string& line) = 0;
};

// Opens a reader for the given file, choosing decompression from the extension:
// ".gz" -> gzip, ".zst" -> zstd, anything else -> plain. Throws
// std::runtime_error if the file cannot be opened.
std::unique_ptr<LineReader> open_line_reader(const std::filesystem::path& path);

} // namespace xbreach::worker
