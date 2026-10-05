#include "../_source/q4b_helpers.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <bit> // std::popcount
#include <climits> // INT_MAX
#include <filesystem>
#include <fstream>
#include <iterator> // std::istreambuf_iterator

const std::filesystem::path TESTS_DIR = "tests";
const std::filesystem::path TEST_ARCHIVE_PATH = TESTS_DIR / "test.q4b";
const std::filesystem::path TEST_ARCHIVE_PATH_2 = TESTS_DIR / "test2.q4b";
const std::filesystem::path TEST_FILE_NAME = "NotoSans-Regular.ttf";
const std::filesystem::path TEST_FILE = "res" / TEST_FILE_NAME;
const std::filesystem::path TEST_FILE_2 = "res/../res" / TEST_FILE_NAME; //TODO: get another file
const std::filesystem::path TEST_FILE_DECOMPRESSED = TESTS_DIR / TEST_FILE_NAME;
const std::filesystem::path TEST_FILE_2_DECOMPRESSED = TESTS_DIR / TEST_FILE_NAME;
const std::filesystem::path TEST_FILE_NONEXISTENT = "nope.txt";
const std::filesystem::path TEST_LIST_FILE = TESTS_DIR / "list.txt";
constexpr int THREAD_COUNT = 4;

// https://stackoverflow.com/questions/6163611/compare-two-files#37575457
static bool FilesAreIdentical(const std::string& p1, const std::string& p2) {
	std::ifstream f1(p1, std::ifstream::binary | std::ifstream::ate);
	std::ifstream f2(p2, std::ifstream::binary | std::ifstream::ate);

	if (f1.fail() || f2.fail()) {
		return false; //file problem
	}

	if (f1.tellg() != f2.tellg()) {
		return false; //size mismatch
	}

	//seek back to beginning and use std::equal to compare contents
	f1.seekg(0, std::ifstream::beg);
	f2.seekg(0, std::ifstream::beg);
	return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
	                  std::istreambuf_iterator<char>(),
	                  std::istreambuf_iterator<char>(f2.rdbuf()));
}

namespace {

TEST(ArchiveStructs, SetPath) {
	q4b::ArchivedFileHeader file_header;
	file_header.setPath("a");

	ASSERT_TRUE(file_header.path[0] == 'a');
	EXPECT_TRUE(file_header.path[q4b::Q4B_MAX_PATH-1] == '\0');

	bool allZeros = true;
	for (int i = 1; i < q4b::Q4B_MAX_PATH-1; i++) {
		if (file_header.path[i] != '\0') {
			allZeros = false;
			break;
		}
	}
	EXPECT_TRUE(allZeros);
	EXPECT_TRUE(file_header.pathIsValid());

	file_header.path[q4b::Q4B_MAX_PATH-2] = 'a';
	EXPECT_FALSE(file_header.pathIsValid());
}

TEST(ArchiveStructs, SetPathLong) {
	q4b::ArchivedFileHeader file_header;

	std::string longPath = "";
	constexpr int longPathLength = 300;
	static_assert(longPathLength > q4b::Q4B_MAX_PATH);

	for (int i = 0; i < longPathLength; i++) {
		longPath += "a";
	}
	file_header.setPath(longPath);

	ASSERT_TRUE(file_header.path[0] == 'a');
	EXPECT_TRUE(file_header.path[q4b::Q4B_MAX_PATH-1] == '\0');
	EXPECT_TRUE(file_header.path[q4b::Q4B_MAX_PATH-2] == 'a');
	EXPECT_TRUE(file_header.pathIsValid());

	file_header.path[q4b::Q4B_MAX_PATH-1] = 'a';
	EXPECT_FALSE(file_header.pathIsValid());
}

TEST(ArchiveStructs, SetPathBackslash) {
	q4b::ArchivedFileHeader file_header;

	std::string backslashPath = TEST_FILE.string();
	std::replace(backslashPath.begin(), backslashPath.end(), '/', '\\');
	file_header.setPath(backslashPath);

	ASSERT_TRUE(file_header.path[0] != '\0');

	bool backslashPresent = false;
	for (int i = 0; i < q4b::Q4B_MAX_PATH; i++) {
		if (file_header.path[i] == '\\') {
			backslashPresent = true;
			break;
		}
	}
	EXPECT_FALSE(backslashPresent);
	EXPECT_TRUE(file_header.pathIsValid());

	file_header.path[0] = '\\';
	EXPECT_FALSE(file_header.pathIsValid());
}

TEST(ArchiveStructs, CompressionFileFlags) {
	// Test constructors
	q4b::CompressionFile file1;
	ASSERT_TRUE(file1.compression_flags == q4b::Q4B_CompressionFileFlags::None);
	q4b::CompressionFile file2 = { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 };
	ASSERT_TRUE(file2.compression_flags == q4b::Q4B_CompressionFileFlags::None);

	// Test one flag set/unset
	q4b::CompressionFile file3;
	file3.setFlag((q4b::Q4B_CompressionFileFlags) 0b01000000);
	ASSERT_TRUE(file3.compression_flags != q4b::Q4B_CompressionFileFlags::None);
	EXPECT_TRUE(file3.getFlag((q4b::Q4B_CompressionFileFlags) 0b01000000));
	EXPECT_TRUE(std::popcount(static_cast<uint32_t>(file3.compression_flags)) == 1);
	file3.unsetFlag((q4b::Q4B_CompressionFileFlags) 0b01000000);
	ASSERT_TRUE(file3.compression_flags == q4b::Q4B_CompressionFileFlags::None);

	// Test multiple flags
	q4b::CompressionFile file4;
	constexpr auto multi_flags = (q4b::Q4B_CompressionFileFlags) 0b01010110;
	file4.setFlag(multi_flags);
	EXPECT_TRUE(file4.compression_flags != q4b::Q4B_CompressionFileFlags::None);
	EXPECT_TRUE(file4.getFlag(multi_flags)); // TODO: Call it getFlags()?
	EXPECT_TRUE(std::popcount(static_cast<uint32_t>(file4.compression_flags)) == 4);
	file4.unsetFlag(multi_flags);
	EXPECT_TRUE(file4.compression_flags == q4b::Q4B_CompressionFileFlags::None);
}


TEST(WriteArchive, NoFiles) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files;
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader));

	q4b::ArchiveHeader header;
	std::vector<q4b::ArchivedFileHeader> list;
	q4b::ReadArchiveHeader(TEST_ARCHIVE_PATH, header, list);

	EXPECT_TRUE(std::equal(header.magic, header.magic + sizeof(header.magic), q4b::MAGIC_NUM));
	EXPECT_TRUE(header.verifyHash());
	EXPECT_TRUE(list.size() == 0);

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchiveAndUnpack, OneFileUncompressed) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_FILE_DECOMPRESSED)) { std::filesystem::remove(TEST_FILE_DECOMPRESSED); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	q4b::UnpackArchive(TEST_ARCHIVE_PATH, TESTS_DIR, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_FILE_DECOMPRESSED));
	EXPECT_TRUE(FilesAreIdentical(TEST_FILE.string(), TEST_FILE_DECOMPRESSED.string()));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_FILE_DECOMPRESSED);
}

TEST(WriteArchive, TwoFilesUncompressed) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE_2, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + 2*sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE) + std::filesystem::file_size(TEST_FILE_2));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, OneFileNonexistent) {
	ASSERT_FALSE(std::filesystem::exists(TEST_FILE_NONEXISTENT));

	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE_NONEXISTENT, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	// Don't write an archive on file loading failure
	EXPECT_FALSE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Check messages
	ASSERT_TRUE(messages.size() == 1);
	EXPECT_TRUE(messages[0].severity == q4b::ErrorSeverity::error);
}

TEST(WriteArchive, SomeFilesExist) {
	ASSERT_FALSE(std::filesystem::exists(TEST_FILE_NONEXISTENT));

	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE_NONEXISTENT, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE_2, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	// Don't write an archive on file loading failure
	EXPECT_FALSE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Check messages
	ASSERT_TRUE(messages.size() == 1);
	EXPECT_TRUE(messages[0].severity == q4b::ErrorSeverity::error);
}

TEST(WriteArchive, ThreeFilesDuplicateFail) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	// Don't write an archive on file loading failure
	ASSERT_FALSE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Check messages
	ASSERT_TRUE(messages.size() == 2);
	EXPECT_TRUE(messages[0].severity == q4b::ErrorSeverity::error);
	EXPECT_TRUE(messages[1].severity == q4b::ErrorSeverity::error);
}

#ifdef Q4B_ENABLE_LZ4
TEST(WriteArchiveAndUnpack, OneFileLz4) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_FILE_DECOMPRESSED)) { std::filesystem::remove(TEST_FILE_DECOMPRESSED); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::lz4, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume LZ4 can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	// Verify LZ4 decompresses correctly
	q4b::UnpackArchive(TEST_ARCHIVE_PATH, TESTS_DIR, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_FILE_DECOMPRESSED));
	EXPECT_TRUE(FilesAreIdentical(TEST_FILE.string(), TEST_FILE_DECOMPRESSED.string()));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_FILE_DECOMPRESSED);
}

#if 0
TEST(WriteArchive, Lz4MetadataSmaller) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}
	if (std::filesystem::exists(TEST_ARCHIVE_PATH_2)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH_2);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::lz4, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));

	files[0].setFlag(q4b::Q4B_CompressionFileFlags::DoWriteMetadata);
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH_2, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH_2));

	// Verify the archive without metadata is smaller
	// (LZ4HC and LZ4F use different compression functions/parameters so the size will be wildly different)
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), std::filesystem::file_size(TEST_ARCHIVE_PATH_2));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_ARCHIVE_PATH_2);
}
#endif
#endif

#ifdef Q4B_ENABLE_ZSTD
TEST(WriteArchiveAndUnpack, OneFileZstd) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_FILE_DECOMPRESSED)) { std::filesystem::remove(TEST_FILE_DECOMPRESSED); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Zstd can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	// Verify Zstd decompresses correctly
	q4b::UnpackArchive(TEST_ARCHIVE_PATH, TESTS_DIR, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_FILE_DECOMPRESSED));
	EXPECT_TRUE(FilesAreIdentical(TEST_FILE.string(), TEST_FILE_DECOMPRESSED.string()));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_FILE_DECOMPRESSED);
}

#ifdef Q4B_ADVANCED_TESTS
TEST(WriteArchive, OneFileZstdMax) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}
	if (std::filesystem::exists(TEST_ARCHIVE_PATH_2)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH_2);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 22 } }; //TODO: get the max level from CompressionSchemeInfo?
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));

	files[0].compression_level = INT_MAX;
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH_2, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH_2));

	// Assume Zstd --max is better
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH_2), std::filesystem::file_size(TEST_ARCHIVE_PATH));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_ARCHIVE_PATH_2);
}
#endif

#if 0
TEST(WriteArchive, ZstdMetadataSmaller) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}
	if (std::filesystem::exists(TEST_ARCHIVE_PATH_2)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH_2);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));

	files[0].setFlag(q4b::Q4B_CompressionFileFlags::DoWriteMetadata);
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH_2, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH_2));

	// Verify the archive without metadata is smaller (TODO: specifically by 4 bytes?)
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), std::filesystem::file_size(TEST_ARCHIVE_PATH_2));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_ARCHIVE_PATH_2);
}
#endif
#endif

#ifdef Q4B_ENABLE_BROTLI
TEST(WriteArchiveAndUnpack, OneFileBrotli) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_FILE_DECOMPRESSED)) { std::filesystem::remove(TEST_FILE_DECOMPRESSED); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::brotli, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Brotli can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	// Verify Brotli decompresses correctly
	q4b::UnpackArchive(TEST_ARCHIVE_PATH, TESTS_DIR, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_FILE_DECOMPRESSED));
	EXPECT_TRUE(FilesAreIdentical(TEST_FILE.string(), TEST_FILE_DECOMPRESSED.string()));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_FILE_DECOMPRESSED);
}
#endif

#ifdef Q4B_ENABLE_SNAPPY
TEST(WriteArchiveAndUnpack, OneFileSnappy) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_FILE_DECOMPRESSED)) { std::filesystem::remove(TEST_FILE_DECOMPRESSED); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::snappy, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Snappy can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	// Verify Snappy decompresses correctly
	q4b::UnpackArchive(TEST_ARCHIVE_PATH, TESTS_DIR, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_FILE_DECOMPRESSED));
	EXPECT_TRUE(FilesAreIdentical(TEST_FILE.string(), TEST_FILE_DECOMPRESSED.string()));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_FILE_DECOMPRESSED);
}
#endif

#ifdef Q4B_ENABLE_STB
TEST(WriteArchiveAndUnpack, OneFileStb) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_FILE_DECOMPRESSED)) { std::filesystem::remove(TEST_FILE_DECOMPRESSED); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::stb, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume stb can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	// Verify stb decompresses correctly
	q4b::UnpackArchive(TEST_ARCHIVE_PATH, TESTS_DIR, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_FILE_DECOMPRESSED));
	EXPECT_TRUE(FilesAreIdentical(TEST_FILE.string(), TEST_FILE_DECOMPRESSED.string()));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_FILE_DECOMPRESSED);
}
#endif

#if defined(Q4B_ENABLE_LZ4) && defined(Q4B_ENABLE_ZSTD)
TEST(WriteArchive, TwoFilesLz4AndZstd) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) { std::filesystem::remove(TEST_ARCHIVE_PATH); }
	if (std::filesystem::exists(TEST_ARCHIVE_PATH_2)) { std::filesystem::remove(TEST_ARCHIVE_PATH_2); }

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::lz4, 1 }, { TEST_FILE_2, q4b::CompressionScheme::lz4, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));

	files[1].data.compression_type = q4b::CompressionScheme::zstd;
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH_2, THREAD_COUNT, &messages);
	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH_2));

	// Assume Zstd compresses the test file more than LZ4
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH_2), std::filesystem::file_size(TEST_ARCHIVE_PATH));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
	std::filesystem::remove(TEST_ARCHIVE_PATH_2);
}
#endif


#if defined(Q4B_ENABLE_LZ4) && defined(Q4B_ENABLE_ZSTD)
TEST(Others, ConvertTextToFileList) {
	std::vector<q4b::CompressionFile> files;
	q4b::ReadArchiveInputFile(TEST_LIST_FILE, files);
	ASSERT_TRUE(files.size() == 7);

	for (const auto& f : files) {
		const std::string path = f.data.path;
		EXPECT_TRUE(path == TEST_FILE.generic_string());
	}

	EXPECT_TRUE(files[0].data.compression_type == q4b::CompressionScheme::lz4);
	EXPECT_TRUE(files[0].compression_level == 1);
	EXPECT_TRUE(files[0].compression_flags == q4b::Q4B_CompressionFileFlags::None);

	EXPECT_TRUE(files[1].data.compression_type == q4b::CompressionScheme::zstd);
	EXPECT_TRUE(files[1].compression_level == -1); // Too big
	EXPECT_TRUE(files[1].compression_flags == q4b::Q4B_CompressionFileFlags::None);

	EXPECT_TRUE(files[2].data.compression_type == q4b::CompressionScheme::brotli);
	EXPECT_TRUE(files[2].compression_level == 5); // Handle extra words
	EXPECT_TRUE(files[2].compression_flags == q4b::Q4B_CompressionFileFlags::None);

	EXPECT_TRUE(files[3].data.compression_type == q4b::CompressionScheme::snappy);
	EXPECT_TRUE(files[3].compression_level == -7); // Don't fix negative values
	EXPECT_TRUE(files[3].compression_flags == q4b::Q4B_CompressionFileFlags::None);

	EXPECT_TRUE(files[4].data.compression_type == q4b::CompressionScheme::stb);
	EXPECT_TRUE(files[4].compression_level == -1); // Set to -1 if not present
	EXPECT_TRUE(files[4].compression_flags == q4b::Q4B_CompressionFileFlags::None);

	// Skip the empty line
	EXPECT_FALSE(std::string(files[5].data.path).empty());

	EXPECT_TRUE(files[5].data.compression_type == q4b::CompressionScheme::Invalid); // Unknown scheme
	EXPECT_TRUE(files[5].compression_level == -1);
	EXPECT_TRUE(files[5].compression_flags == q4b::Q4B_CompressionFileFlags::None);

	EXPECT_TRUE(files[6].data.compression_type == q4b::CompressionScheme::Invalid); // No scheme
	EXPECT_TRUE(files[6].compression_level == -1);
	EXPECT_TRUE(files[6].compression_flags == q4b::Q4B_CompressionFileFlags::None);
}
#endif

} // namespace
