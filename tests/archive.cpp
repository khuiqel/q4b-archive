#include "../_source/q4b.hpp"
#include <gtest/gtest.h>

#include <filesystem>
#include <algorithm>

const std::filesystem::path TEST_ARCHIVE_PATH = "tests/test.q4b";
const std::filesystem::path TEST_FILE = "res/NotoSans-Regular.ttf";

namespace {

//TODO: if fs error then do something

TEST(WriteArchive, NoFiles) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::CompressionFile> files;
	q4b::WriteArchive(files, TEST_ARCHIVE_PATH);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, OneFileUncompressed) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0, 0 } };
	q4b::WriteArchive(files, TEST_ARCHIVE_PATH);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, OneFileCompressedZstd) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 1, 0 } };
	q4b::WriteArchive(files, TEST_ARCHIVE_PATH);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Zstd can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, TwoFilesUncompressed) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0, 0 }, { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0, 0 } };
	q4b::WriteArchive(files, TEST_ARCHIVE_PATH);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + 2*sizeof(q4b::ArchivedFileHeader) + 2*std::filesystem::file_size(TEST_FILE));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, TwoFilesCompressedZstd) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 1, 0 }, { TEST_FILE, q4b::CompressionScheme::zstd, 1, 0 } };
	q4b::WriteArchive(files, TEST_ARCHIVE_PATH);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Zstd can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + 2*sizeof(q4b::ArchivedFileHeader) + 2*std::filesystem::file_size(TEST_FILE));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

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

} // namespace
