#include "../_source/q4b.hpp"
#include <gtest/gtest.h>

#include <filesystem>
#include <algorithm>
#include <bit> //std::popcount

const std::filesystem::path TEST_ARCHIVE_PATH = "tests/test.q4b";
const std::filesystem::path TEST_ARCHIVE_PATH_2 = "tests/test2.q4b";
const std::filesystem::path TEST_FILE = "res/NotoSans-Regular.ttf";
const std::filesystem::path TEST_FILE_2 = "res/../res/NotoSans-Regular.ttf"; //TODO: get another file
const std::filesystem::path TEST_FILE_NONEXISTANT = "nope.txt";
constexpr int THREAD_COUNT = 4;

namespace {

TEST(WriteArchive, NoFiles) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files;
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, OneFileUncompressed) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, OneFileCompressedZstd) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Zstd can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, TwoFilesUncompressed) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE_2, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	EXPECT_EQ(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + 2*sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE) + std::filesystem::file_size(TEST_FILE_2));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, TwoFilesCompressedZstd) {
	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::zstd, 1 }, { TEST_FILE_2, q4b::CompressionScheme::zstd, 1 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	ASSERT_TRUE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Assume Zstd can compress the test file to less than its original size
	EXPECT_LT(std::filesystem::file_size(TEST_ARCHIVE_PATH), sizeof(q4b::ArchiveHeader) + 2*sizeof(q4b::ArchivedFileHeader) + std::filesystem::file_size(TEST_FILE) + std::filesystem::file_size(TEST_FILE_2));

	std::filesystem::remove(TEST_ARCHIVE_PATH);
}

TEST(WriteArchive, OneFileNonexistant) {
	ASSERT_FALSE(std::filesystem::exists(TEST_FILE_NONEXISTANT));

	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE_NONEXISTANT, q4b::CompressionScheme::Uncompressed, 0 } };
	q4b::WriteArchive(files, ".", TEST_ARCHIVE_PATH, THREAD_COUNT, &messages);

	// Don't write an archive on file loading failure
	EXPECT_FALSE(std::filesystem::exists(TEST_ARCHIVE_PATH));
	// Check messages
	ASSERT_TRUE(messages.size() == 1);
	EXPECT_TRUE(messages[0].severity == q4b::ErrorSeverity::error);
}

TEST(WriteArchive, SomeFilesExist) {
	ASSERT_FALSE(std::filesystem::exists(TEST_FILE_NONEXISTANT));

	if (std::filesystem::exists(TEST_ARCHIVE_PATH)) {
		std::filesystem::remove(TEST_ARCHIVE_PATH);
	}

	std::vector<q4b::ErrorMessage> messages;
	std::vector<q4b::CompressionFile> files = { { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE_NONEXISTANT, q4b::CompressionScheme::Uncompressed, 0 }, { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 } };
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
	ASSERT_TRUE(file1.compression_flags == 0);
	q4b::CompressionFile file2 = { TEST_FILE, q4b::CompressionScheme::Uncompressed, 0 };
	ASSERT_TRUE(file2.compression_flags == 0);

	// Test one flag set/unset
	q4b::CompressionFile file3;
	file3.setFlag((q4b::Q4B_CompressionFileFlags) 0b01000000);
	ASSERT_TRUE(file3.compression_flags != 0);
	EXPECT_TRUE(file3.getFlag((q4b::Q4B_CompressionFileFlags) 0b01000000));
	EXPECT_TRUE(std::popcount(file3.compression_flags) == 1);
	file3.unsetFlag((q4b::Q4B_CompressionFileFlags) 0b01000000);
	ASSERT_TRUE(file3.compression_flags == 0);

	// Test multiple flags
	q4b::CompressionFile file4;
	constexpr auto multi_flags = (q4b::Q4B_CompressionFileFlags) 0b01010110;
	file4.setFlag(multi_flags);
	EXPECT_TRUE(file4.compression_flags != 0);
	EXPECT_TRUE(file4.getFlag(multi_flags)); // TODO: Call it getFlags()?
	EXPECT_TRUE(std::popcount(file4.compression_flags) == 4);
	file4.unsetFlag(multi_flags);
	EXPECT_TRUE(file4.compression_flags == 0);
}

} // namespace
