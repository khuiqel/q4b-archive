#include "q4b_helpers.hpp"

#include <algorithm>
#include <cstring> //memcpy
#include <fstream>
#include <iostream>

namespace q4b {

CompressionSchemeInfo* SCHEME_INFO[] = {
	new CompressionSchemeInfo_Uncompressed(),
	#ifdef Q4B_ENABLE_LZ4
	new CompressionSchemeInfo_Lz4(),
	#endif
	#ifdef Q4B_ENABLE_ZSTD
	new CompressionSchemeInfo_Zstd(),
	#endif
	#ifdef Q4B_ENABLE_BROTLI
	new CompressionSchemeInfo_Brotli(),
	#endif
	#ifdef Q4B_ENABLE_SNAPPY
	new CompressionSchemeInfo_Snappy(),
	#endif
	#ifdef Q4B_ENABLE_STB
	new CompressionSchemeInfo_Stb(),
	#endif
};
//TODO: verify there are exactly the right amount of schemes, C++ doesn't really support this

CompressionSchemeFunctions* SchemeToFunctions(CompressionScheme scheme) {
	switch (scheme) {
		case q4b::CompressionScheme::Uncompressed: [[fallthrough]];
		default:
			return nullptr;

		#ifdef Q4B_ENABLE_LZ4
		case q4b::CompressionScheme::lz4:
			return new CompressionSchemeFunctions_Lz4();
		#endif

		#ifdef Q4B_ENABLE_ZSTD
		case q4b::CompressionScheme::zstd:
			return new CompressionSchemeFunctions_Zstd();
		#endif

		#ifdef Q4B_ENABLE_BROTLI
		case q4b::CompressionScheme::brotli:
			return new CompressionSchemeFunctions_Brotli();
		#endif

		#ifdef Q4B_ENABLE_SNAPPY
		case q4b::CompressionScheme::snappy:
			return new CompressionSchemeFunctions_Snappy();
		#endif

		#ifdef Q4B_ENABLE_STB
		case q4b::CompressionScheme::stb:
			return new CompressionSchemeFunctions_Stb();
		#endif
	}
}

void ExistencePrune(std::vector<CompressionFile>& file_list) noexcept {
	auto it = std::remove_if(file_list.begin(), file_list.end(),
		[](const auto& file) {
			std::error_code ec;
			return !std::filesystem::exists(file.data.path, ec);
		}
	);
	file_list.erase(it, file_list.end());
}

// Don't delete the return value!
static CompressionSchemeInfo* SchemeToInfo(CompressionScheme scheme) {
	for (CompressionSchemeInfo* info : SCHEME_INFO) {
		if (scheme == info->scheme) {
			return info;
		}
	}
	return nullptr;
}

template <bool extraFeatures, bool GenericExport>
std::vector<std::pair<ArchivedFileHeader, void*>> CompressFiles_internal(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	// Check for valid compression schemes and existence
	{
		bool allSchemesValid = true;
		for (int i = 0; i < file_list.size(); i++) {
			if (!SchemeIsEnabled(file_list[i].data.compression_type)) {
				messages->push_back({ ErrorSeverity::error, "Unavailable scheme: " + std::string(SchemeToDisplayStr(file_list[i].data.compression_type)) });
				allSchemesValid = false;
			}
		}
		if (!allSchemesValid) {
			return {};
		}

		bool allFilesExist = true;
		for (int i = 0; i < file_list.size(); i++) {
			if (!std::filesystem::exists(root_file_path / file_list[i].data.path)) {
				messages->push_back({ ErrorSeverity::error, std::string(file_list[i].data.path) + " doesn't exist" });
				allFilesExist = false;
				//TODO: maybe this function should copy file_list to prevent someone modifying the files
			}
		}
		if (!allFilesExist) {
			return {};
		}
	}

	// Compress files
	std::vector<std::pair<ArchivedFileHeader, void*>> compressed_files_data; compressed_files_data.reserve(file_list.size());

	for (int i = 0; i < file_list.size(); i++) {
		if constexpr (extraFeatures)
			if (exit_flag->load(std::memory_order_acquire)) [[unlikely]] {
				messages->push_back({ ErrorSeverity::info, "Quitting early" });
				return compressed_files_data;
			}

		const CompressionFile& file = file_list[i];
		void* file_data;
		int64_t file_size = LoadFileIntoMemory(root_file_path / file.data.path, &file_data);
		if (file_size == -1) [[unlikely]] {
			messages->push_back({ ErrorSeverity::error, "Could not load file \"" + (root_file_path / file.data.path).string() + "\"" });
			compressed_files_data.push_back({ {}, nullptr });
			continue;
		}

		ArchivedFileHeader file_header;
		std::memcpy(file_header.path, file.data.path, Q4B_MAX_PATH);
		file_header.compression_type = file.data.compression_type;
		file_header.uncompressed_size = file_size;
		file_header.flags = 0;

		if (file.data.compression_type == CompressionScheme::Uncompressed) {
			file_header.compressed_size = file_header.uncompressed_size;
			if constexpr (!GenericExport)
				file_header.compressed_hash = file_header.uncompressed_hash = ComputeHash(file_data, file_header.uncompressed_size);
			compressed_files_data.push_back({ file_header, file_data });
		} else {
			CompressionSchemeFunctions* functions = SchemeToFunctions(file.data.compression_type);
			if (functions == nullptr) [[unlikely]] {
				messages->push_back({ ErrorSeverity::error, "Unknown compression type for file \"" + (root_file_path / file.data.path).string() + "\"" });
				compressed_files_data.push_back({ {}, nullptr });
				delete[] file_data;
			} else {
				CompressionSchemeInfo* info = SchemeToInfo(file.data.compression_type);
				//TODO
				if (!info->usableForGenericExport && file.getFlag(q4b::Q4B_CompressionFileFlags::DoWriteMetadata)) {
					messages->push_back({ ErrorSeverity::warn, std::string(info->displayName) + " doesn't support writing metadata" });
				}

				void* outputData;
				uint64_t compressedSize;
				if constexpr (GenericExport) {
					compressedSize = functions->Compress_GenericExport(file.compression_level, (q4b::Q4B_CompressionFileFlags)file.compression_flags, file_data, file_size, &outputData);
				} else {
					compressedSize = functions->Compress(              file.compression_level, (q4b::Q4B_CompressionFileFlags)file.compression_flags, file_data, file_size, &outputData);
				}
				//TODO: handle errors

				file_header.compressed_size = compressedSize;
				if constexpr (!GenericExport) {
					file_header.uncompressed_hash = ComputeHash(file_data, file_header.uncompressed_size);
					file_header.compressed_hash = ComputeHash(outputData, file_header.compressed_size);
				}
				compressed_files_data.push_back({ file_header, outputData });

				delete functions;
				delete[] file_data;
			}
		}

		if constexpr (extraFeatures) files_completed->fetch_add(1, std::memory_order_release);
	}
	//TODO: maybe push an info message for each one compressed

	return compressed_files_data;
}
template std::vector<std::pair<ArchivedFileHeader, void*>> CompressFiles_internal<false, false>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;
template std::vector<std::pair<ArchivedFileHeader, void*>> CompressFiles_internal<false, true>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;
template std::vector<std::pair<ArchivedFileHeader, void*>> CompressFiles_internal<true, false>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;
template std::vector<std::pair<ArchivedFileHeader, void*>> CompressFiles_internal<true, true>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path,
	std::vector<ErrorMessage>* messages,
	const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

template <bool extraFeatures>
void WriteCompressedFiles_internal(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output_dir,
	std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	// Check for duplicate filenames
	// Valid schemes and existence are checked in CompressFiles_internal()
	{
		bool duplicatesExist = false;
		std::vector<std::string> sorted_list; sorted_list.reserve(file_list.size());
		std::transform(file_list.begin(), file_list.end(),
			std::back_inserter(sorted_list),
			[](const CompressionFile& file) { return std::filesystem::path(file.data.path).filename().string(); }
		);
		std::sort(sorted_list.begin(), sorted_list.end(), [](const auto& lhs, const auto& rhs) {
			return lhs < rhs;
		});
		for (int i = 1; i < file_list.size(); i++) {
			if (sorted_list[i-1] == sorted_list[i]) {
				messages->push_back({ ErrorSeverity::error, sorted_list[i] + " has a duplicate" });
				duplicatesExist = true;
			}
		}
		if (duplicatesExist) {
			if constexpr (extraFeatures) working_flag->store(false);
			return;
		}
	}

	// Compress files
	auto compressed_files_data = CompressFiles_internal<extraFeatures, true>(
		file_list, root_file_path,
		messages,
		exit_flag, files_completed);

	if (compressed_files_data.size() != file_list.size()) {
		//TODO
	}

	if constexpr (extraFeatures)
		if (exit_flag->load(std::memory_order_acquire)) [[unlikely]] {
			messages->push_back({ ErrorSeverity::info, "Quitting early" });
			for (auto& [header, f] : compressed_files_data) {
				if (f) delete[] f;
			}
			working_flag->store(false);
			return;
		}

	// Do not proceed further if there was an error
	for (const auto& message : *messages) {
		if (message.severity == ErrorSeverity::error) {
			for (auto& [header, f] : compressed_files_data) {
				if (f) delete[] f;
			}
			if constexpr (extraFeatures) working_flag->store(false);
			return;
		}
	}

	// Write files
	for (auto& [header, f] : compressed_files_data) {
		CompressionSchemeInfo* info = SchemeToInfo(header.compression_type);
		const std::string output = (output_dir / std::filesystem::path(header.path).filename()).string() + info->fileExtensions[0];
		std::ofstream outfile(output, std::ios::binary);
		outfile.write((const char*)f, header.compressed_size);
	}

	// Cleanup
	for (int i = 0; i < file_list.size(); i++) {
		// The pointer can only be nullptr if there was an error
		delete[] compressed_files_data[i].second;
	}
	if constexpr (extraFeatures) working_flag->store(false);
}
template void WriteCompressedFiles_internal<true>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output_dir,
	std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;
template void WriteCompressedFiles_internal<false>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output_dir,
	std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

template <bool extraFeatures>
void WriteArchive_internal(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
	int threadCount, std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	// Check for valid paths and duplicate paths
	// Valid schemes and existence are checked in CompressFiles_internal()
	{
		bool allFilesExist = true;
		for (int i = 0; i < file_list.size(); i++) {
			if (!file_list[i].data.pathIsValid()) {
				messages->push_back({ ErrorSeverity::error, "File idx " + std::to_string(i) + " is invalid" });
				allFilesExist = false;
			}
		}
		if (!allFilesExist) {
			if constexpr (extraFeatures) working_flag->store(false);
			return;
		}

		bool duplicatesExist = false;
		std::vector<std::string> sorted_list; sorted_list.reserve(file_list.size());
		std::transform(file_list.begin(), file_list.end(),
			std::back_inserter(sorted_list),
			[](const CompressionFile& file) { return file.data.path; }
		);
		std::sort(sorted_list.begin(), sorted_list.end(), [](const auto& lhs, const auto& rhs) {
			return lhs < rhs;
		});
		for (int i = 1; i < file_list.size(); i++) {
			if (sorted_list[i-1] == sorted_list[i]) {
				messages->push_back({ ErrorSeverity::error, sorted_list[i] + " has a duplicate" });
				duplicatesExist = true;
			}
		}
		if (duplicatesExist) {
			if constexpr (extraFeatures) working_flag->store(false);
			return;
		}
	}

	// Open file
	const std::filesystem::path output_tmp = output.string() + ".tmp";
	std::ofstream outfile(output_tmp, std::ios::binary);
	if (!outfile) {
		// TODO: Is std::atomic_thread_fence needed for messages? Or is it fine because because the default memory order (memory_order_seq_cst) on working_flag forces a fence?
		messages->push_back({ ErrorSeverity::error, "Could not reserve temp file" });
		if constexpr (extraFeatures) working_flag->store(false);
		return;
	}

	// Compress files
	auto compressed_files_data = CompressFiles_internal<extraFeatures, false>(
		file_list, root_file_path,
		messages,
		exit_flag, files_completed);

	if (compressed_files_data.size() != file_list.size()) {
		//TODO
	}

	if constexpr (extraFeatures)
		if (exit_flag->load(std::memory_order_acquire)) [[unlikely]] {
			messages->push_back({ ErrorSeverity::info, "Quitting early" });
			for (auto& [header, f] : compressed_files_data) {
				if (f) delete[] f;
			}
			working_flag->store(false);
			return;
		}

	// Do not proceed further if there was an error
	for (const auto& message : *messages) {
		if (message.severity == ErrorSeverity::error) {
			for (auto& [header, f] : compressed_files_data) {
				if (f) delete[] f;
			}
			if constexpr (extraFeatures) working_flag->store(false);
			return;
		}
	}

	// Write archive
	ArchiveHeader ah;
	ah.num_files = file_list.size();
	ah.computeHash();
	SwapEndiannessIfNeeded(ah);
	outfile.write((const char*)&ah, sizeof(ah));

	for (auto& [header, f] : compressed_files_data) {
		SwapEndiannessIfNeeded(header);
		outfile.write((const char*)&header, sizeof(header));
	}
	for (auto& [header, f] : compressed_files_data) {
		outfile.write((const char*)f, header.compressed_size);
	}

	outfile.close();
	std::error_code ec;
	std::filesystem::rename(output_tmp, output, ec);
	if (ec) {
		messages->push_back({ ErrorSeverity::error, "Could not convert the temp file to real file" });
		// std::filesystem::remove(output_tmp);
	}

	// Cleanup
	for (int i = 0; i < file_list.size(); i++) {
		// The pointer can only be nullptr if there was an error
		delete[] compressed_files_data[i].second;
	}
	if constexpr (extraFeatures) working_flag->store(false);
}
template void WriteArchive_internal<true>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
	int threadCount, std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;
template void WriteArchive_internal<false>(
	const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
	int threadCount, std::vector<ErrorMessage>* messages,
	std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept;

void DecodeArchive(const std::filesystem::path& input, const std::filesystem::path& output) noexcept {
	if (std::filesystem::exists(output)) {
		if (!std::filesystem::is_directory(output)) {
			return;
		}
	} else {
		std::filesystem::create_directory(output);
	}

	size_t archive_size = std::filesystem::file_size(input);
	if (archive_size < sizeof(ArchiveHeader)) {
		return;
	}

	void* archive;
	int64_t file_size = LoadFileIntoMemory(input, &archive);
	if (file_size == -1) {
		//TODO
		return;
	}

	ArchiveHeader ah;
	std::memcpy(&ah, archive, sizeof(ArchiveHeader));
	SwapEndiannessIfNeeded(ah);
	if (!std::equal(ah.magic, ah.magic + sizeof(ah.magic), MAGIC_NUM)) {
		delete[] archive;
		return;
	}
	if (ah.num_files == 0) {
		delete[] archive;
		return;
	}
	if (!ah.verifyHash()) {
		std::cout << "hash didn't match on header\n";
		//return;
	}
	// if (ah.version <= 0) { return; } //TODO: versioning

	std::vector<void*> compressed_files_data(ah.num_files);
	std::vector<ArchivedFileHeader> compressed_files_headers(ah.num_files);
	size_t file_offset = sizeof(ArchiveHeader);

	for (int i = 0; i < ah.num_files; i++) {
		if (archive_size < file_offset + sizeof(ArchivedFileHeader)) {
			delete[] archive;
			std::cout << "insufficient size for headers\n";
			return;
		}

		ArchivedFileHeader& file_header = compressed_files_headers[i];
		std::memcpy(&file_header, (const char*)archive + file_offset, sizeof(ArchivedFileHeader));
		SwapEndiannessIfNeeded(file_header);
		file_offset += sizeof(ArchivedFileHeader);
	}

	for (int i = 0; i < ah.num_files; i++) {
		size_t compressed_size = compressed_files_headers[i].compressed_size;
		if (archive_size < file_offset + compressed_size) {
			delete[] archive;
			for (int j = 0; j < i; j++) {
				delete[] compressed_files_data[i];
			}
			std::cout << "insufficient size for files\n";
			return;
		}

		compressed_files_data[i] = new char[compressed_size];
		std::memcpy(compressed_files_data[i], (const char*)archive + file_offset, compressed_size);
		file_offset += compressed_size;
	}
	// std::cout << "archive size: " << archive_size << " | file offset: " << file_offset << std::endl;

	for (int i = 0; i < ah.num_files; i++) {
		auto hash = ComputeHash(compressed_files_data[i], compressed_files_headers[i].compressed_size);
		if (hash != compressed_files_headers[i].compressed_hash) {
			delete[] archive;
			for (void* f : compressed_files_data) {
				delete[] f;
			}
			// std::cout << "size: " << compressed_files_headers[i].compressed_size << " | hash stored: " << compressed_files_headers[i].compressed_hash << " | computed hash: " << hash << std::endl;
			std::cout << "hash didn't match on file " << i << std::endl;
			return;
		}
	}

	for (int i = 0; i < ah.num_files; i++) {
		const ArchivedFileHeader& file_header = compressed_files_headers[i];

		CompressionSchemeFunctions* functions = SchemeToFunctions(file_header.compression_type);
		if (file_header.compression_type == CompressionScheme::Uncompressed) {
			std::ofstream outfile(output / std::filesystem::path(file_header.path).filename(), std::ios::binary);
			outfile.write((const char*)compressed_files_data[i], file_header.compressed_size);
			outfile.close();
			//TODO: should probably check hashes and size again, since the compressed_size could not equal the uncompressed size on ill-formatted data
		} else if (functions == nullptr) {
			std::cerr << "ERROR: Unknown compression: " << q4b::SchemeToDisplayStr(file_header.compression_type) << " (" << (uint32_t)file_header.compression_type << ")" << std::endl;
		} else {
			void* outputData;
			uint64_t decompressedSize = functions->Decompress(compressed_files_data[i], file_header.compressed_size, &outputData, file_header.uncompressed_size);
			delete functions;
			if (decompressedSize != file_header.uncompressed_size) {
				std::cout << "file size mismatch!\n";
				//TODO
			}
			std::ofstream outfile(output / std::filesystem::path(file_header.path).filename(), std::ios::binary);
			outfile.write((const char*)outputData, decompressedSize);
			outfile.close();
		}

		// std::cout << "uncompressed " << i << "\n";
	}

	std::cout << "unpacked .q4b\n";

	for (void* f : compressed_files_data) {
		delete[] f;
	}
	delete[] archive;
}

bool ReadArchiveHeader(const std::filesystem::path& input, ArchiveHeader& header, std::vector<ArchivedFileHeader>& list) noexcept {
	size_t archive_size = std::filesystem::file_size(input);
	if (archive_size < sizeof(ArchiveHeader)) {
		return false;
	}

	void* archive;
	int64_t file_size = LoadFileIntoMemory(input, &archive); //TODO: no need to load the entire file...
	if (file_size == -1) {
		//TODO
		return false;
	}
	if (file_size < sizeof(ArchiveHeader)) {
		delete[] archive;
		return false;
	}
	std::memcpy(&header, archive, sizeof(ArchiveHeader));
	SwapEndiannessIfNeeded(header);

	size_t file_offset = sizeof(ArchiveHeader);
	for (int i = 0; i < header.num_files; i++) {
		if (archive_size < file_offset + sizeof(ArchivedFileHeader)) {
			delete[] archive;
			std::cout << "insufficient size for headers\n";
			return false;
		}

		ArchivedFileHeader file_header;
		std::memcpy(&file_header, (const char*)archive + file_offset, sizeof(ArchivedFileHeader));
		SwapEndiannessIfNeeded(file_header);
		list.push_back(file_header);
		file_offset += sizeof(ArchivedFileHeader);
	}

	delete[] archive;
	return true;
}

int64_t LoadFileIntoMemory(const std::filesystem::path& filepath, void** dest) noexcept {
	// Open file (at the end)
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);
	if (!file) {
		return -1;
	}

	// Get file size: don't use std::filesystem::file_size because the size *could* change
	//file.seekg(0, std::ios::end); // Not needed because the file was opened at the end
	int64_t fileSize = file.tellg();
	if (fileSize == -1) {
		return -1;
	}
	file.seekg(0, std::ios::beg);
	if (!file) {
		return -1;
	}

	// Read
	char* buffer = new char[fileSize];
	file.read(buffer, fileSize);
	if (!file) {
		delete[] buffer;
		return -1;
	}

	// Return
	int64_t bytesRead = file.gcount();
	if (bytesRead != fileSize) {
		delete[] buffer;
		return -1;
	}
	*dest = buffer;
	return bytesRead;

	// No need to call file.close() because fstream destructors close automatically
}

// Returns 1 on failure, sets scheme on success
int ExtToScheme(const std::string& FILE_EXT, CompressionScheme* scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		for (const auto& ext : info->fileExtensions) {
			if (FILE_EXT == ext) {
				*scheme = info->scheme;
				return 0;
			}
		}
	}
	std::cerr << "ERROR: could not determine scheme\n";
	return 1;
}

// Returns 1 on failure, sets scheme on success
int StrToScheme(const std::string& SCHEME, CompressionScheme* scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		for (const auto& name : info->searchNames) {
			if (SCHEME == name) {
				*scheme = info->scheme;
				return 0;
			}
		}
	}
	std::cerr << "ERROR: unknown scheme\n";
	return 1;
}

std::string SchemeToFileExt(CompressionScheme scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		if (scheme == info->scheme) {
			return info->fileExtensions[0];
		}
	}
	return "";
}

const char* SchemeToDisplayStr(CompressionScheme scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		if (scheme == info->scheme) {
			return info->displayName;
		}
	}
	return "Unknown";
}

void ReadArchiveInputFile(const std::filesystem::path& input, std::vector<CompressionFile>& file_list) {
	std::ifstream f(input);
	std::string line;
	while (std::getline(f, line)) {
		size_t pos_start, pos_end;
		pos_start = line.find_first_of(" ");
		std::string filename = line.substr(0, pos_start);
		pos_end = line.find_first_of(" ", pos_start+1);
		std::string scheme = line.substr(pos_start+1, pos_end - pos_start - 1);
		pos_start = pos_end;
		pos_end = line.find_first_of(" ", pos_start+1);
		std::string level = line.substr(pos_start+1, pos_end - pos_start - 1);

		CompressionScheme s;
		StrToScheme(scheme, &s);
		file_list.push_back({ filename, s, std::stoi(level) });
	}
}

} // namespace q4b
