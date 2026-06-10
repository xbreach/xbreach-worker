#include "io/file_reader.hpp"

#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <stdexcept>

namespace xbreach::worker {

namespace {

constexpr std::size_t kReadChunk = 64 * 1024;

// Splits a byte stream produced by a subclass into lines. Subclasses only need
// to supply the next decompressed chunk via read_chunk().
class BufferedLineReader : public LineReader {
  public:
    bool next_line(std::string& line) override {
        for (;;) {
            const std::size_t newline = buffer_.find('\n', position_);
            if (newline != std::string::npos) {
                emit(line, position_, newline - position_);
                position_ = newline + 1;
                return true;
            }
            if (source_exhausted_) {
                if (position_ < buffer_.size()) {
                    emit(line, position_, buffer_.size() - position_);
                    position_ = buffer_.size();
                    return true;
                }
                return false;
            }
            // Drop already-consumed bytes so the buffer stays bounded.
            if (position_ > 0) {
                buffer_.erase(0, position_);
                position_ = 0;
            }
            std::string chunk;
            if (read_chunk(chunk) == 0) {
                source_exhausted_ = true;
            } else {
                buffer_ += chunk;
            }
        }
    }

  protected:
    // Appends the next decompressed bytes to `out`. Returns the number of bytes
    // appended; 0 means the source is exhausted.
    virtual std::size_t read_chunk(std::string& out) = 0;

  private:
    void emit(std::string& line, std::size_t offset, std::size_t length) const {
        line.assign(buffer_, offset, length);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
    }

    std::string buffer_;
    std::size_t position_ = 0;
    bool source_exhausted_ = false;
};

class PlainSource final : public BufferedLineReader {
  public:
    explicit PlainSource(const std::filesystem::path& path) : stream_(path, std::ios::binary) {
        if (!stream_) {
            throw std::runtime_error("failed to open file: " + path.string());
        }
    }

  protected:
    std::size_t read_chunk(std::string& out) override {
        std::array<char, kReadChunk> buffer{};
        stream_.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream_.gcount();
        if (count <= 0) {
            return 0;
        }
        out.append(buffer.data(), static_cast<std::size_t>(count));
        return static_cast<std::size_t>(count);
    }

  private:
    std::ifstream stream_;
};

class GzipSource final : public BufferedLineReader {
  public:
    explicit GzipSource(const std::filesystem::path& path)
        : file_(gzopen(path.string().c_str(), "rb")) {
        if (file_ == nullptr) {
            throw std::runtime_error("failed to open gzip file: " + path.string());
        }
    }

    ~GzipSource() override {
        if (file_ != nullptr) {
            gzclose(file_);
        }
    }

    GzipSource(const GzipSource&) = delete;
    GzipSource& operator=(const GzipSource&) = delete;

  protected:
    std::size_t read_chunk(std::string& out) override {
        std::array<char, kReadChunk> buffer{};
        const int count = gzread(file_, buffer.data(), static_cast<unsigned>(buffer.size()));
        if (count < 0) {
            int errnum = 0;
            const char* message = gzerror(file_, &errnum);
            throw std::runtime_error(std::string("gzip read error: ") + message);
        }
        if (count == 0) {
            return 0;
        }
        out.append(buffer.data(), static_cast<std::size_t>(count));
        return static_cast<std::size_t>(count);
    }

  private:
    gzFile file_;
};

class ZstdSource final : public BufferedLineReader {
  public:
    explicit ZstdSource(const std::filesystem::path& path)
        : stream_(path, std::ios::binary), dstream_(ZSTD_createDStream()) {
        if (!stream_) {
            throw std::runtime_error("failed to open zstd file: " + path.string());
        }
        if (dstream_ == nullptr) {
            throw std::runtime_error("failed to create zstd decompression stream");
        }
        ZSTD_initDStream(dstream_);
        in_buffer_.resize(ZSTD_DStreamInSize());
        out_buffer_.resize(ZSTD_DStreamOutSize());
    }

    ~ZstdSource() override {
        if (dstream_ != nullptr) {
            ZSTD_freeDStream(dstream_);
        }
    }

    ZstdSource(const ZstdSource&) = delete;
    ZstdSource& operator=(const ZstdSource&) = delete;

  protected:
    std::size_t read_chunk(std::string& out) override {
        for (;;) {
            if (in_pos_ >= in_size_ && !file_exhausted_) {
                stream_.read(in_buffer_.data(), static_cast<std::streamsize>(in_buffer_.size()));
                in_size_ = static_cast<std::size_t>(stream_.gcount());
                in_pos_ = 0;
                if (in_size_ == 0) {
                    file_exhausted_ = true;
                }
            }
            if (in_pos_ >= in_size_ && file_exhausted_) {
                return 0;
            }

            ZSTD_inBuffer input{in_buffer_.data(), in_size_, in_pos_};
            ZSTD_outBuffer output{out_buffer_.data(), out_buffer_.size(), 0};
            const std::size_t result = ZSTD_decompressStream(dstream_, &output, &input);
            in_pos_ = input.pos;
            if (ZSTD_isError(result)) {
                throw std::runtime_error(std::string("zstd decompress error: ") +
                                         ZSTD_getErrorName(result));
            }
            if (output.pos > 0) {
                out.append(out_buffer_.data(), output.pos);
                return output.pos;
            }
            // No output produced yet; loop to feed more compressed input.
        }
    }

  private:
    std::ifstream stream_;
    ZSTD_DStream* dstream_;
    std::string in_buffer_;
    std::string out_buffer_;
    std::size_t in_size_ = 0;
    std::size_t in_pos_ = 0;
    bool file_exhausted_ = false;
};

std::string lower_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace

std::unique_ptr<LineReader> open_line_reader(const std::filesystem::path& path) {
    const std::string ext = lower_extension(path);
    if (ext == ".gz") {
        return std::make_unique<GzipSource>(path);
    }
    if (ext == ".zst") {
        return std::make_unique<ZstdSource>(path);
    }
    return std::make_unique<PlainSource>(path);
}

} // namespace xbreach::worker
