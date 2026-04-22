/* NOTICE:
 * The archive portions of this header were heavily inspired by the P3A format:
 * https://github.com/ph3at/p3a-format/blob/main/p3a.h
 * License: MIT
 *
 * There are only so many ways to make a good file format, so it looks very similar because it
 * is very similar.
 * The CompressionScheme enum was explicitly matched though, to make hypothetical future
 * compatibility easier.
 */

#pragma once
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <type_traits>

#include <xxhash.h>

namespace q4b {

inline constexpr uint32_t Q4B_VERSION_GEN(int major, int minor) {
	return major*1000 + minor;
}

constexpr uint32_t Q4B_ARCHIVE_VERSION = Q4B_VERSION_GEN(0, 0);
constexpr int Q4B_MAX_PATH = 256;
constexpr char MAGIC_NUM[8] = "Q4B_YAY";

enum class CompressionScheme : uint64_t {
	Uncompressed = 0,
	lz4,
	zstd,
	zstd_dict,
	CountNormal,

	CountExtraStart = 100,
	//lz4 has a dict mode, probably don't bother
	brotli,
	lzma,
	bzip2,
	miniz, //zlib replacement
	//OpenZL, //https://github.com/facebook/openzl
	CountExtraEnd,
};

inline const char* CompressionToStr(CompressionScheme c) {
	switch (c) {
		default: return "Unknown";

		case CompressionScheme::Uncompressed: return "Uncompressed";
		case CompressionScheme::lz4:          return "lz4";
		case CompressionScheme::zstd:         return "zstd";
		case CompressionScheme::zstd_dict:    return "zstd_dict";

		// case CompressionScheme::brotli:       return "brotli";
		// case CompressionScheme::lzma:         return "lzma";
		// case CompressionScheme::bzip2:        return "bzip2";
		// case CompressionScheme::miniz:        return "miniz";
	}
}

inline XXH64_hash_t ComputeHash(void* data, size_t size) {
	return XXH64(data, size, 0);
	//XXH3 can do 64- or 128-bit hashes, and 128-bit is unnecessary
}

struct ArchiveHeader {
	char magic[8];
	uint32_t flags;
	uint32_t version;
	uint64_t num_files;
	XXH64_hash_t self_hash;

	ArchiveHeader();
	void computeHash(); // Call after creating!
	bool verifyHash() const;
};
static_assert(sizeof(ArchiveHeader) == (8+4+4+8+8));
static_assert(std::is_trivially_copyable<ArchiveHeader>::value);

struct ArchivedFileHeader {
	char path[Q4B_MAX_PATH];
	CompressionScheme compression_type;
	uint64_t compressed_size;
	uint64_t uncompressed_size;
	//uint64_t offset; //TODO: offset from start of archive to compressed data
	XXH64_hash_t compressed_hash;
	XXH64_hash_t uncompressed_hash;
};
static_assert(sizeof(ArchivedFileHeader) == (Q4B_MAX_PATH+8+8+8+8+8));
static_assert(std::is_trivially_copyable<ArchivedFileHeader>::value);

//TODO: remember to dump in LE!

//note: this is for the application, the previous one is for the archive
struct CompressionFile {
	std::filesystem::path filepath;
	CompressionScheme compression_type;
	int32_t compression_level;
	int32_t compression_flags; //TODO

	inline const char* getFilepath() const {
		// This function exists because MSVC doesn't know how to convert std::filesystem::path::c_str() to a C-str
		return (const char*) filepath.string().c_str();
	}
};



/* Removes the files that no longer exist.
 *
 * @param file_list [in,out] List of files to process.
 *
 * @return void
 */
void ExistencePrune(std::vector<CompressionFile>& file_list) noexcept;

/* Returns a list of the files sorted by their filesize.
 *
 * @param file_list [in] List of files to process.
 *
 * @return A new list of files, sorted from largest to smallest.
 */
std::vector<CompressionFile> GetFileListSortedBySize(const std::vector<CompressionFile>& file_list) noexcept;

/* Writes the Q4B archive.
 *
 * @param file_list [in] List of files to process.
 * @param output [in] Output name of the archive.
 *
 * @return void
 */
void WriteArchive(const std::vector<CompressionFile>& file_list, const std::filesystem::path& output) noexcept;

/* Decodes a Q4B archive.
 *
 * @param input [in] Name of the archive.
 * @param output [in] Output folder of the archive.
 *
 * @return void
 */
void DecodeArchive(const std::filesystem::path& input, const std::filesystem::path& output) noexcept;

/* Reads the header of a Q4B archive.
 *
 * @param input [in] Name of the archive.
 * @param header [in,out] Where to put the archive's header.
 * @param list [in,out] Where to put the archive's list of files.
 *
 * @return True on success, false on failure.
 */
bool ReadArchiveHeader(const std::filesystem::path& input, ArchiveHeader& header, std::vector<ArchivedFileHeader>& list) noexcept;

/* Loads a file into memory. Returns the pointer to the allocated memory.
 *
 * @param filepath [in] The file to load.
 * @param dest [out] The pointer for where the file will be put, allocated using `new[]`. Will not be set on error.
 *
 * @return Size of the file. -1 if error. If the full file couldn't be loaded, returns -1.
 */
[[nodiscard]] int64_t LoadFileIntoMemory(const std::filesystem::path& filepath, char** dest) noexcept;

/* Compresses data in memory using Zstd. Returns the size of the compressed data, as a Zstd block (meaning no metadata). (TODO: for now it's a frame, so there is metadata)
 *
 * @param file_data [in] The file in memory.
 * @param uncompressed_size [in] The file's size.
 * @param compression_level [in] Zstd compression level to use.
 * @param compressed_file [out] The pointer for where the compressed data will be put.
 *
 * @return Size of the compressed data. -1 if it failed (TODO). Allocated memory will be >= the compressed size.
 */
[[nodiscard]] size_t CompressZstdData(const void* file_data, size_t file_size, int compression_level, char** compressed_file) noexcept;

/* Compresses data in memory using LZ4. Returns the size of the compressed data, as a LZ4 block (meaning no metadata).
 *
 * @param file_data [in] The file in memory.
 * @param uncompressed_size [in] The file's size.
 * @param compression_level [in] LZ4 compression level to use. (TODO: not used currently)
 * @param compressed_file [out] The pointer for where the compressed data will be put.
 *
 * @return Size of the compressed data. -1 if it failed (TODO). Allocated memory will be >= the compressed size.
 */
[[nodiscard]] size_t CompressLz4Data(const void* file_data, size_t file_size, int compression_level, char** compressed_file) noexcept;

/* Uncompresses data in memory using Zstd. Returns the decompressed size.
 * 
 * @param file_data [in] The file in memory.
 * @param compressed_size [in] The file's size.
 * @param decompressed_file [out] The pointer for where the compressed data will be put.
 * @param decompressed_file_size [out] The size of the decompressed data. This function was made to read Zstd blocks, so there is no frame to grab the size from.
 *
 * @return Size of the decompressed data. -1 if it failed (TODO). Should be equal to decompressed_file_size.
 */
[[nodiscard]] size_t UncompressZstdData(const void* file_data, size_t compressed_file_size, char** decompressed_file, size_t decompressed_file_size) noexcept;

/* Uncompresses data in memory using LZ4.
 * 
 * @param file_data [in] The file in memory.
 * @param compressed_size [in] The file's size.
 * @param decompressed_file [out] The pointer for where the compressed data will be put.
 * @param decompressed_file_size [out] The size of the decompressed data. This function was made to read LZ4 blocks, so there is no frame to grab the size from.
 *
 * @return Size of the decompressed data. -1 if it failed (TODO). Should be equal to decompressed_file_size.
 */
[[nodiscard]] size_t UncompressLz4Data(const void* file_data, size_t compressed_file_size, char** decompressed_file, size_t decompressed_file_size) noexcept;

} // namespace q4b
