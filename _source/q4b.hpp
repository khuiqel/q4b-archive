/* NOTICE:
 * The Q4B archive format was heavily inspired by the P3A format:
 * https://github.com/ph3at/p3a-format/blob/main/p3a.h
 * License: MIT
 *
 * There are only so many ways to make a good file format, so it looks very similar because it
 * is very similar.
 * The CompressionScheme enum was explicitly matched though, to make hypothetical future
 * compatibility easier (except as a u32 instead of u64).
 */

#pragma once
#include <cstdint>
#include <filesystem>
#include <type_traits> // std::is_trivially_copyable
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

	CountExtraStart = 1000, // Schemes after this are not used by P3A
	brotli,
	lzma,
	bzip2,
	zlib, //implemented using miniz
	lz4_dict,
	stb,
	//OpenZL, //https://github.com/facebook/openzl
	CountExtraEnd,
};

constexpr CompressionScheme LIST_OF_ENABLED_SCHEMES[] = {
	CompressionScheme::Uncompressed,
	#ifdef Q4B_ENABLE_LZ4
	CompressionScheme::lz4,
	#endif
	#ifdef Q4B_ENABLE_ZSTD
	CompressionScheme::zstd,
	#endif
	#ifdef Q4B_ENABLE_BROTLI
	CompressionScheme::brotli,
	#endif
	#ifdef Q4B_ENABLE_STB
	CompressionScheme::stb,
	#endif
};
constexpr size_t ENABLED_SCHEMES_COUNT = std::size(LIST_OF_ENABLED_SCHEMES);

inline bool SchemeIsEnabled(CompressionScheme c) {
	// If you want an O(1) lookup instead of O(n), use a switch statement
	for (CompressionScheme s : LIST_OF_ENABLED_SCHEMES) {
		if (s == c) return true;
	}
	return false;
}

inline const char* CompressionToStr(CompressionScheme c) {
	switch (c) {
		default: return "Unknown";

		case CompressionScheme::Uncompressed: return "Uncompressed";
		case CompressionScheme::lz4:          return "LZ4";
		case CompressionScheme::zstd:         return "Zstd";
		// case CompressionScheme::zstd_dict:    return "Zstd_dict";

		case CompressionScheme::brotli:       return "Brotli";
		// case CompressionScheme::lzma:         return "LZMA";
		// case CompressionScheme::bzip2:        return "bzip2";
		// case CompressionScheme::zlib:         return "zlib";
		// case CompressionScheme::lz4_dict:     return "LZ4_dict";
		case CompressionScheme::stb:          return "stb";
	}
}

inline XXH64_hash_t ComputeHash(void* data, size_t size) {
	return XXH64(data, size, 0);
	//XXH3 can do 64- or 128-bit hashes, and 128-bit is unnecessary
}

#pragma pack(push, 1)

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

#pragma pack(pop)

enum class Q4B_CompressionFileFlags : uint32_t {
	None                         = 0,
	DoWriteMetadata              = 1 << 0,
	TreatFileAsAlreadyCompressed = 1 << 1, // TODO: How to handle the uncompressed size? Add unknown size flag to the file header?
};

inline uint32_t operator&(Q4B_CompressionFileFlags lhs, Q4B_CompressionFileFlags rhs) { return static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs); }
inline uint32_t operator|(Q4B_CompressionFileFlags lhs, Q4B_CompressionFileFlags rhs) { return static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs); }

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

} // namespace q4b
