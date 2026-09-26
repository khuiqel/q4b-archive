#pragma once
#include "q4b.hpp"
#include "app/compression_info.hpp"
#include "lib/compression_data.hpp"

#include <atomic>
#include <bit>
#include <concepts>
#include <filesystem>
#include <unordered_map>
#include <utility> // std::pair
#include <vector>

namespace q4b {

extern CompressionSchemeInfo* SCHEME_INFO[ENABLED_SCHEMES_COUNT];

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
		data.setPath(file.string());
		data.compression_type = compression_type_;
		data.flags = 0;
		compression_level = compression_level_;
		compression_flags = 0;
	}
};

/* Converts the endianness to/from LE.
 *
 * @tparam bytes Data type
 * @param input [in] Data
 *
 * @return Data byte-swapped on BE, data unchanged on LE.
 */
template <std::unsigned_integral bytes>
constexpr bytes ConvertHostEndianToLittleEndian(bytes input) {
	if constexpr (std::endian::native == std::endian::little) {
		return input;
	} else if constexpr (std::endian::native == std::endian::big) {
		return std::byteswap(input);
	} else {
		// #error "unknown endianness"
		return input;
	}
}

inline void SwapEndiannessIfNeeded(ArchiveHeader& ah) {
	ah.flags     = ConvertHostEndianToLittleEndian(ah.flags);
	ah.version   = ConvertHostEndianToLittleEndian(ah.version);
	ah.num_files = ConvertHostEndianToLittleEndian(ah.num_files);
	ah.self_hash = ConvertHostEndianToLittleEndian(ah.self_hash);
}
inline void SwapEndiannessIfNeeded(ArchivedFileHeader& header) {
	header.flags             = ConvertHostEndianToLittleEndian(header.flags);
	header.compression_type  = (CompressionScheme)ConvertHostEndianToLittleEndian((uint32_t)header.compression_type);
	header.compressed_size   = ConvertHostEndianToLittleEndian(header.compressed_size);
	header.uncompressed_size = ConvertHostEndianToLittleEndian(header.uncompressed_size);
	header.compressed_hash   = ConvertHostEndianToLittleEndian(header.compressed_hash);
	header.uncompressed_hash = ConvertHostEndianToLittleEndian(header.uncompressed_hash);
}

/* Translates the compression scheme to the compression/decompression functions.
 *
 * @param scheme [in] Compression scheme.
 *
 * @return Pointer to functions struct, allocated using `new`. nullptr on failure.
 */
CompressionSchemeFunctions* SchemeToFunctions(CompressionScheme scheme);

/* Removes the files that no longer exist.
 *
 * @param file_list [in,out] List of files to process.
 *
 * @return void
 */
void ExistencePrune(std::vector<CompressionFile>& file_list) noexcept;

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

/* Internal function for writing archives and quick compression. Won't begin on certain failures
 * (like invalid scheme or nonexistence). Will continue if the compression encounters an error.
 * Does not have a flag for signalling when it's done because it's not supposed to be used by itself.
 * TODO: another tparam for quitting early on error?
 *
 * @tparam GenericExport Does the generic export version of compress. Will not calculate the ArchivedFileHeader hashes.
 *
 * @return A list of the files compressed, allocated using `new[]`, nullptr on failure.
 */
template <bool extraFeatures, bool GenericExport>
std::vector<std::pair<ArchivedFileHeader, void*>> CompressFiles_internal(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

template <bool extraFeatures>
void WriteCompressedFiles_internal(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output_dir,
	std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

/* Writes compressed files.
 *
 * @param file_list [in] List of files to process. Does NOT allow duplicate filenames.
 * @param root_file_path [in] The root which `file_list` is relative to.
 * @param output_dir [in] Output folder for the files.
 * @param messages [out,optional] Accumulated error messages. (TODO)
 * @param working_flag [out] Flag to signal if the function is still running.
 * @param exit_flag [in] Flag to signal to the function if it should exit early.
 * @param files_completed [out] Count of files compressed so far.
 *
 * @return void
 */
inline void WriteCompressedFiles(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output_dir,
                                 std::vector<ErrorMessage>* messages,
                                 std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	WriteCompressedFiles_internal<true>(file_list, root_file_path, output_dir, messages, working_flag, exit_flag, files_completed);
}

/* Writes compressed files.
 *
 * @param file_list [in] List of files to process. Does NOT allow duplicate filenames.
 * @param root_file_path [in] The root which `file_list` is relative to.
 * @param output_dir [in] Output folder for the files.
 * @param messages [out,optional] Accumulated error messages. (TODO)
 *
 * @return void
 */
inline void WriteCompressedFiles(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output_dir,
                                 std::vector<ErrorMessage>* messages) noexcept {

	WriteCompressedFiles_internal<false>(file_list, root_file_path, output_dir, messages, nullptr, nullptr, nullptr);
}

template <bool extraFeatures>
void WriteArchive_internal(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
                           int threadCount, std::vector<ErrorMessage>* messages,
                           std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

/* Writes the Q4B archive.
 *
 * @param file_list [in] List of files to process. Does NOT allow duplicate filepaths.
 * @param root_file_path [in] The root which `file_list` is relative to.
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
 * @param root_file_path [in] The root which `file_list` is relative to.
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

/* Internal function for decoding archives. Exits early if the input file is malformed, such as too
 * small to hold all its reported data, or some other error when reading the input file. Will
 * continue if the decompression encounters an error.
 * Does not have a flag for signalling when it's done because it's not supposed to be used by itself.
 *
 * @return A list of the files decompressed, allocated using `new[]`, nullptr on failure.
 */
template <bool extraFeatures>
std::vector<std::pair<ArchivedFileHeader, void*>> DecompressArchive_internal(
	const std::filesystem::path& input,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

template <bool extraFeatures>
void UnpackArchive_internal(const std::filesystem::path& input, const std::filesystem::path& output,
                            std::vector<ErrorMessage>* messages,
                            std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

/* Unpacks a Q4B archive.
 *
 * @param input [in] Name of the archive.
 * @param output [in] Output folder for the files.
 * @param messages [out,optional] Accumulated error messages. (TODO)
 * @param working_flag [out] Flag to signal if the function is still running.
 * @param exit_flag [in] Flag to signal to the function if it should exit early.
 * @param files_completed [out] Count of files compressed so far.
 *
 * @return void
 */
inline void UnpackArchive(const std::filesystem::path& input, const std::filesystem::path& output,
                          std::vector<ErrorMessage>* messages,
                          std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	UnpackArchive_internal<true>(input, output, messages, working_flag, exit_flag, files_completed);
}

/* Unpacks a Q4B archive.
 *
 * @param input [in] Name of the archive.
 * @param output [in] Output folder for the files.
 * @param messages [out,optional] Accumulated error messages. (TODO)
 *
 * @return void
 */
inline void UnpackArchive(const std::filesystem::path& input, const std::filesystem::path& output,
                          std::vector<ErrorMessage>* messages) noexcept {
	UnpackArchive_internal<false>(input, output, messages, nullptr, nullptr, nullptr);
}

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
[[nodiscard]] int64_t LoadFileIntoMemory(const std::filesystem::path& filepath, void** dest) noexcept;

// Returns 1 on failure, sets scheme on success
int ExtToScheme(const std::string& FILE_EXT, CompressionScheme* scheme);

// Returns 1 on failure, sets scheme on success
int StrToScheme(const std::string& SCHEME, CompressionScheme* scheme);

// Returns an empty string if the scheme doesn't exist
std::string SchemeToFileExt(q4b::CompressionScheme scheme);

// Returns "Unknown" for unknown/disabled schemes
const char* SchemeToDisplayStr(CompressionScheme scheme);

/* Reads a text file containing files to be compressed. Intended for the CLI.
 *
 * @param input [in] The text file. Format is "filename scheme level" per line. (TODO: support spaces in filename, also zstd --max... maybe this function just splits on spaces?)
 * @param file_list [out] Output file list.
 *
 * @return void
 * TODO: errors
 */
void ReadArchiveInputFile(const std::filesystem::path& input, std::vector<CompressionFile>& file_list);

} // namespace q4b
