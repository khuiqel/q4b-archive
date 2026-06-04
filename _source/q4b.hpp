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
#include <cstdint>
#include <type_traits>
#include <atomic>

#include <xxhash.h>

namespace q4b {

inline constexpr uint32_t Q4B_VERSION_GEN(int major, int minor) {
	return major*1000 + minor;
}

constexpr uint32_t Q4B_ARCHIVE_VERSION = Q4B_VERSION_GEN(0, 0);
constexpr int Q4B_MAX_PATH = 256;
constexpr char MAGIC_NUM[8] = "Q4B_YAY";

enum class CompressionScheme : uint32_t {
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

enum class Q4B_ArchivedFileFlags : uint32_t {
	None                         = 0,
	MetadataEmbedded             = 1 << 0,
};

struct ArchivedFileHeader {
	char path[Q4B_MAX_PATH];
	uint32_t flags;
	CompressionScheme compression_type;
	uint64_t compressed_size;
	uint64_t uncompressed_size;
	//uint64_t offset; //TODO: offset from start of archive to compressed data
	XXH64_hash_t compressed_hash;
	XXH64_hash_t uncompressed_hash;
	//TODO: self_hash?

	inline void setFlag(Q4B_ArchivedFileFlags flag) { flags |= static_cast<uint32_t>(flag); }
	inline void unsetFlag(Q4B_ArchivedFileFlags flag) { flags &= ~static_cast<uint32_t>(flag); }
	inline bool getFlag(Q4B_ArchivedFileFlags flag) const { return flags & static_cast<uint32_t>(flag); }

	bool pathIsValid() const; // Returns true for: 1. no backslashes; 2. last byte in array is \0 (to be able to make it a string); 3. end of string to end of array is \0 //TODO: another function to check for NTFS-invalid characters
	void setPath(const std::filesystem::path& path); // Writes the path, replacing backslashes and filling the remainder of the array with \0
};
static_assert(sizeof(ArchivedFileHeader) == (Q4B_MAX_PATH+4+4+8+8+8+8));
static_assert(std::is_trivially_copyable<ArchivedFileHeader>::value);

//TODO: remember to dump in LE!

enum class Q4B_CompressionFileFlags : uint32_t {
	None                         = 0,
	DoWriteMetadata              = 1 << 0,
	TreatFileAsAlreadyCompressed = 1 << 1, // TODO: How to handle the uncompressed size? Add unknown size flag to the file header?
};

//note: this is for the application, the previous one is for the archive
struct CompressionFile {
	ArchivedFileHeader data;
	int32_t compression_level;
	uint32_t compression_flags;

	inline const char* getFilepath() const {
		return data.path;
	}

	inline void setFlag(Q4B_CompressionFileFlags flag) { compression_flags |= static_cast<uint32_t>(flag); }
	inline void unsetFlag(Q4B_CompressionFileFlags flag) { compression_flags &= ~static_cast<uint32_t>(flag); }
	inline bool getFlag(Q4B_CompressionFileFlags flag) const { return compression_flags & static_cast<uint32_t>(flag); }

	CompressionFile() {
		data.setPath("");
		data.compression_type = CompressionScheme::Uncompressed;
		data.flags = 0;
		compression_level = 0;
		compression_flags = 0;
	}
	CompressionFile(const std::filesystem::path& file, CompressionScheme compression_type_, int32_t compression_level_) {
		data.setPath(file);
		data.compression_type = compression_type_;
		data.flags = 0;
		compression_level = compression_level_;
		compression_flags = 0;
	}
};

enum class ErrorSeverity {
	Unknown,
	info,
	warn,
	error,
	Count
};

struct ErrorMessage {
	ErrorSeverity severity;
	std::string msg;
};



/* Removes the files that no longer exist.
 *
 * @param file_list [in,out] List of files to process.
 *
 * @return void
 */
void ExistencePrune(std::vector<CompressionFile>& file_list) noexcept;

template <bool extraFeatures>
void WriteArchive_internal(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
                           int threadCount, std::vector<ErrorMessage>* messages,
                           std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

/* Writes the Q4B archive.
 *
 * @param file_list [in] List of files to process. Does NOT allow duplicate filepaths.
 * @param root_file_path [in] The root which file_list is relative to.
 * @param output [in] Output name of the archive.
 * @param threadCount [in] Number of threads to use, counting the starter thread.
 * @param messages [out,optional] Accumulated error messages. (TODO)
 * @param working_flag [out] Flag to signal if the function is still running.
 * @param exit_flag [in] Flag to signal to the function if it should exit early.
 * @param files_completed [out] Count of files compressed so far.
 *
 * @return void
 */
inline void WriteArchive(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
                         int threadCount, std::vector<ErrorMessage>* messages,
                         std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	WriteArchive_internal<true>(file_list, root_file_path, output, threadCount, messages, working_flag, exit_flag, files_completed);
}

/* Writes the Q4B archive.
 *
 * @param file_list [in] List of files to process. Does NOT allow duplicate filepaths.
 * @param root_file_path [in] The root which file_list is relative to.
 * @param output [in] Output name of the archive.
 * @param threadCount [in] Number of threads to use, counting the starter thread.
 * @param messages [out,optional] Accumulated error messages. (TODO)
 *
 * @return void
 */
inline void WriteArchive(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
                         int threadCount, std::vector<ErrorMessage>* messages) noexcept {
	WriteArchive_internal<false>(file_list, root_file_path, output, threadCount, messages, nullptr, nullptr, nullptr);
}

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
 * @param header [out] Where to put the archive's header.
 * @param list [out] Where to put the archive's list of files.
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

/* Compresses data in memory using Zstd. Returns the size of the compressed data, including its frame metadata if requested.
 *
 * @param cctx [in] The Zstd context. Will be cast from void* to ZSTD_CCtx*.
 * @param file_data [in] The file in memory.
 * @param uncompressed_size [in] The file's size.
 * @param compressed_file [out] The pointer for where the compressed data will be put.
 *
 * @return Size of the compressed data. -1 if it failed (TODO). Allocated memory will be >= the compressed size.
 */
[[nodiscard]] size_t CompressZstdData(void* cctx, const void* file_data, size_t uncompressed_size, char** compressed_file) noexcept;

/* Decompresses data in memory using Zstd. Returns the decompressed size.
 *
 * @param file_data [in] The file in memory.
 * @param compressed_size [in] The file's size.
 * @param decompressed_file [out] The pointer for where the compressed data will be put.
 * @param decompressed_size [in] The size of the decompressed data. This function was made to read Zstd blocks, so there is no frame to grab the size from. (Can decompress frames though.)
 *
 * @return Size of the decompressed data. -1 if it failed (TODO). Should be equal to decompressed_file_size.
 */
[[nodiscard]] size_t DecompressZstdData(const void* file_data, size_t compressed_size, char** decompressed_file, size_t decompressed_size) noexcept;

/* Compresses data in memory using LZ4. Returns the size of the compressed data, as a LZ4 block (meaning no metadata).
 *
 * @param file_data [in] The file in memory.
 * @param uncompressed_size [in] The file's size. NOTE: LZ4 has a limit of ~2GB (LZ4_MAX_INPUT_SIZE).
 * @param compressed_file [out] The pointer for where the compressed data will be put.
 * @param compression_level [in] LZ4 compression level to use.
 *
 * @return Size of the compressed data. -1 if it failed (TODO). Allocated memory will be >= the compressed size.
 *         NOTE: This returns a wildly different size from CompressLz4Data_Metadata() because they use different functions.
 */
[[nodiscard]] int CompressLz4Data(const void* file_data, int uncompressed_size, char** compressed_file, int compression_level) noexcept;

/* Compresses data in memory using LZ4. Returns the size of the compressed data, as a LZ4 frame (meaning there's metadata).
 *
 * @param prefs [in] The LZ4F preferences. Will be cast from void* to LZ4F_preferences_t*.
 * @param file_data [in] The file in memory.
 * @param uncompressed_size [in] The file's size. NOTE: LZ4 has a limit of ~2GB (LZ4_MAX_INPUT_SIZE)... however, it doesn't appear LZ4 frames have the same limitation.
 * @param compressed_file [out] The pointer for where the compressed data will be put.
 *
 * @return Size of the compressed data. -1 if it failed (TODO). Allocated memory will be >= the compressed size.
 *         NOTE: This returns a wildly different size from CompressLz4Data() because they use different functions.
 */
[[nodiscard]] size_t CompressLz4Data_Metadata(const void* prefs, const void* file_data, size_t uncompressed_size, char** compressed_file) noexcept;

/* Decompresses data in memory using LZ4. Only works for LZ4 blocks (meaning no metadata).
 *
 * @param file_data [in] The file in memory.
 * @param compressed_size [in] The file's size.
 * @param decompressed_file [out] The pointer for where the compressed data will be put.
 * @param decompressed_size [in] The size of the decompressed data. This function was made to read LZ4 blocks, so there is no frame to grab the size from.
 *
 * @return Size of the decompressed data. -1 if it failed (TODO). Should be equal to decompressed_file_size.
 */
[[nodiscard]] int DecompressLz4Data(const void* file_data, int compressed_size, char** decompressed_file, size_t decompressed_size) noexcept;

/* Decompresses data in memory using LZ4. Only works for LZ4 frames (meaning there's metadata).
 *
 * @param file_data [in] The file in memory.
 * @param compressed_size [in] The file's size.
 * @param decompressed_file [out] The pointer for where the compressed data will be put.
 * @param decompressed_size [in] The size of the decompressed data. This function was not made to read the metadata from an LZ4 frame.
 *
 * @return Size of the decompressed data. -1 if it failed (TODO). Should be equal to decompressed_file_size.
 */
[[nodiscard]] size_t DecompressLz4Data_Metadata(const void* file_data, size_t compressed_size, char** decompressed_file, size_t decompressed_size) noexcept;

} // namespace q4b
