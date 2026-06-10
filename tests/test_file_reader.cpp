#include <gtest/gtest.h>

#include <zlib.h>
#include <zstd.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "io/file_reader.hpp"

using xbreach::worker::open_line_reader;

namespace {

namespace fs = std::filesystem;

std::vector<std::string> read_all(const fs::path& path) {
    auto reader = open_line_reader(path);
    std::vector<std::string> lines;
    std::string line;
    while (reader->next_line(line)) {
        lines.push_back(line);
    }
    return lines;
}

fs::path temp_file(const std::string& name) {
    return fs::temp_directory_path() / ("xbreach_worker_test_" + name);
}

void write_gzip(const fs::path& path, const std::string& content) {
    gzFile file = gzopen(path.string().c_str(), "wb");
    ASSERT_NE(file, nullptr);
    gzwrite(file, content.data(), static_cast<unsigned>(content.size()));
    gzclose(file);
}

void write_zstd(const fs::path& path, const std::string& content) {
    std::vector<char> compressed(ZSTD_compressBound(content.size()));
    const std::size_t written =
        ZSTD_compress(compressed.data(), compressed.size(), content.data(), content.size(), 3);
    ASSERT_FALSE(ZSTD_isError(written));
    std::ofstream out(path, std::ios::binary);
    out.write(compressed.data(), static_cast<std::streamsize>(written));
}

} // namespace

TEST(FileReader, ReadsPlainTextAndStripsCarriageReturns) {
    const fs::path path = temp_file("plain.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "first\nsecond\r\nthird"; // last line has no trailing newline
    }
    const auto lines = read_all(path);
    fs::remove(path);
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "first");
    EXPECT_EQ(lines[1], "second");
    EXPECT_EQ(lines[2], "third");
}

TEST(FileReader, DoesNotEmitEmptyTrailingLine) {
    const fs::path path = temp_file("trailing.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "a\nb\n";
    }
    const auto lines = read_all(path);
    fs::remove(path);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[1], "b");
}

TEST(FileReader, ReadsGzip) {
    const fs::path path = temp_file("data.gz");
    write_gzip(path, "alpha\nbeta\ngamma\n");
    const auto lines = read_all(path);
    fs::remove(path);
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "alpha");
    EXPECT_EQ(lines[2], "gamma");
}

TEST(FileReader, ReadsZstd) {
    const fs::path path = temp_file("data.zst");
    write_zstd(path, "one\ntwo\nthree\nfour\n");
    const auto lines = read_all(path);
    fs::remove(path);
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "one");
    EXPECT_EQ(lines[3], "four");
}

TEST(FileReader, HandlesManyLinesAcrossChunks) {
    const fs::path path = temp_file("many.gz");
    constexpr int kCount = 20'000;
    std::string content;
    for (int i = 0; i < kCount; ++i) {
        content += "line" + std::to_string(i) + "\n";
    }
    write_gzip(path, content);
    const auto lines = read_all(path);
    fs::remove(path);
    ASSERT_EQ(lines.size(), static_cast<std::size_t>(kCount));
    EXPECT_EQ(lines.front(), "line0");
    EXPECT_EQ(lines.back(), "line19999");
}

TEST(FileReader, ThrowsOnMissingFile) {
    EXPECT_THROW(open_line_reader(temp_file("does_not_exist_zzz.txt")), std::runtime_error);
}
