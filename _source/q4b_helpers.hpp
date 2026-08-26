#pragma once
#include "q4b.hpp"
#include "lib/compression_data.hpp"

#include <atomic>
#include <unordered_map>
#include <vector>

namespace q4b {

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

} // namespace q4b
