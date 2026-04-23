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

} // namespace
