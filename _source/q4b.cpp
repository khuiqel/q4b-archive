#include "q4b.hpp"

#include <fstream>
#include <iostream>
#include <cstring> //memcpy
#include <algorithm>

#include <zstd.h>
#include <lz4hc.h>
#include <lz4frame.h>
#include <brotli/encode.h>
#include <brotli/decode.h>

namespace q4b {

ArchiveHeader::ArchiveHeader() {
	std::memcpy(magic, MAGIC_NUM, sizeof(MAGIC_NUM));
	flags = 0;
	version = Q4B_ARCHIVE_VERSION;
	num_files = 0;
	self_hash = 0; // Exists to make sure the header isn't corrupted
}

void ArchiveHeader::computeHash() {
	self_hash = 0;
	self_hash = ComputeHash(this, sizeof(*this));
}

bool ArchiveHeader::verifyHash() const {
	ArchiveHeader copied(*this);
	copied.computeHash();
	return copied.self_hash == this->self_hash;
}

bool ArchivedFileHeader::pathIsValid() const {
	// No backslashes
	if (std::find(path, path + Q4B_MAX_PATH, '\\') != (path + Q4B_MAX_PATH)) {
		return false;
	}

	// It can be made into a string
	if (path[Q4B_MAX_PATH-1] != '\0') {
		return false;
	}

	// Rest of array is filled with zero
	uint32_t firstZero = std::find(path, path + Q4B_MAX_PATH, 0) - path;
	if (!std::all_of(path + firstZero, path + Q4B_MAX_PATH, [](auto val) { return val == 0; })) {
		return false;
	}

	return true;
};

void ArchivedFileHeader::setPath(const std::filesystem::path& path) {
	const std::string p = path.string();
	size_t charCount = std::min(p.size(), size_t(Q4B_MAX_PATH-1));
	std::copy(p.begin(), p.begin() + charCount, this->path);
	std::replace(this->path, this->path + charCount, '\\', '/');
	std::fill(this->path + charCount, this->path + Q4B_MAX_PATH, '\0');
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
//probably put this on a different thread

// Adapted from setMaxCompression(ZSTD_compressionParameters*) in programs/zstdcli.c
static inline void zstd_setMaxCompression(ZSTD_CCtx* cctx) {
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog,        ZSTD_cParam_getBounds(ZSTD_c_windowLog).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_chainLog,         ZSTD_cParam_getBounds(ZSTD_c_chainLog).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_hashLog,          ZSTD_cParam_getBounds(ZSTD_c_hashLog).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_searchLog,        ZSTD_cParam_getBounds(ZSTD_c_searchLog).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_minMatch,         ZSTD_cParam_getBounds(ZSTD_c_minMatch).lowerBound); // lowerBound
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_targetLength,     ZSTD_cParam_getBounds(ZSTD_c_targetLength).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_strategy,         ZSTD_cParam_getBounds(ZSTD_c_strategy).upperBound);

	ZSTD_CCtx_setParameter(cctx, ZSTD_c_overlapLog,       ZSTD_cParam_getBounds(ZSTD_c_overlapLog).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_ldmHashLog,       ZSTD_cParam_getBounds(ZSTD_c_ldmHashLog).upperBound);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_ldmHashRateLog,   0);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_ldmMinMatch,      16);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_ldmBucketSizeLog, ZSTD_cParam_getBounds(ZSTD_c_ldmBucketSizeLog).upperBound);

	// Setting ZSTD_c_enableLongDistanceMatching is *sometimes* necessary, despite claiming to be auto-set by windowLog and strategy
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLongDistanceMatching, 1);
	// Does not appear necessary to set the compression level
	// ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, ZSTD_maxCLevel());
}



template <bool extraFeatures>
void WriteArchive_internal(const std::vector<CompressionFile>& file_list, const std::filesystem::path& root_file_path, const std::filesystem::path& output,
                  int threadCount, std::vector<ErrorMessage>* messages,
                  std::atomic_bool* working_flag, const std::atomic_bool* exit_flag, std::atomic_int* files_completed) noexcept {

	// Open file
	const std::filesystem::path output_tmp = output.string() + ".tmp";
	std::ofstream outfile(output_tmp, std::ios::binary);
	if (!outfile) {
		// TODO: Is std::atomic_thread_fence needed for messages? Or is it fine because because the default memory order (memory_order_seq_cst) on working_flag forces a fence?
		messages->push_back({ ErrorSeverity::error, "Could not reserve temp file" });
		if constexpr (extraFeatures) working_flag->store(false);
		return;
	}

	// Check for existence and duplicates
	{
		bool allFilesExist = true;
		for (int i = 0; i < file_list.size(); i++) {
			if (!file_list[i].data.pathIsValid()) {
				messages->push_back({ ErrorSeverity::error, "File idx " + std::to_string(i) + " is invalid" });
				allFilesExist = false;
				//TODO: maybe this function should copy file_list to prevent someone modifying the files
			} else if (!std::filesystem::exists(root_file_path / file_list[i].data.path)) {
				messages->push_back({ ErrorSeverity::error, std::string(file_list[i].data.path) + " doesn't exist" });
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

	// Compress files
	std::vector<char*> compressed_files_data(file_list.size());
	std::vector<ArchivedFileHeader> compressed_files_headers(file_list.size());
	threadCount = (threadCount > 0) ? (threadCount-1) : 0;

	for (int i = 0; i < file_list.size(); i++) {
		if constexpr (extraFeatures)
			if (exit_flag->load(std::memory_order_acquire)) [[unlikely]] {
				messages->push_back({ ErrorSeverity::info, "Quitting early" });
				for (int j = 0; j < i; j++) {
					delete[] compressed_files_data[j];
				}
				working_flag->store(false);
				return;
			}

		const CompressionFile& file = file_list[i];
		char* file_data;
		int64_t file_size = LoadFileIntoMemory(root_file_path / file.data.path, &file_data);
		if (file_size == -1) [[unlikely]] {
			messages->push_back({ ErrorSeverity::error, "Could not load file \"" + (root_file_path / file.data.path).string() + "\"" });
			for (int j = 0; j < i; j++) {
				delete[] compressed_files_data[j];
			}
			if constexpr (extraFeatures) working_flag->store(false);
			return;
		}

		ArchivedFileHeader& file_header = compressed_files_headers[i];
		std::memcpy(file_header.path, file.data.path, Q4B_MAX_PATH);
		file_header.compression_type = file.data.compression_type;
		file_header.uncompressed_size = file_size;
		file_header.uncompressed_hash = ComputeHash(file_data, file_header.uncompressed_size);

		switch (file.data.compression_type) {
			default:
				messages->push_back({ ErrorSeverity::warn, "Unknown compression type for file \"" + (root_file_path / file.data.path).string() + "\"" });
				file_header.compression_type = CompressionScheme::Uncompressed;
				[[fallthrough]];
			case CompressionScheme::Uncompressed: {
				compressed_files_data[i] = file_data;
				file_header.compressed_size = file_header.uncompressed_size;
				file_header.compressed_hash = file_header.uncompressed_hash;
				break;
			}

			case CompressionScheme::zstd: {
				//TODO: this should use a fixed Zstd context instead
				ZSTD_CCtx* cctx = ZSTD_createCCtx();
				ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, threadCount);
				if (file.compression_level == INT_MAX) [[unlikely]] {
					zstd_setMaxCompression(cctx);
				} else {
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, file.compression_level);
					// Other parameters are automatically set given the compression level (that would've been a huge headache otherwise)
				}

				if (file.getFlag(Q4B_CompressionFileFlags::DoWriteMetadata)) {
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 1); // Default already 1
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_dictIDFlag, 0); // Not necessary
					file_header.setFlag(Q4B_ArchivedFileFlags::MetadataEmbedded);
				} else {
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 0); // Default already 0
					ZSTD_CCtx_setParameter(cctx, ZSTD_c_dictIDFlag, 0); // Not necessary
				}

				size_t compressed_size = CompressZstdData(cctx, file_data, file_header.uncompressed_size, &(compressed_files_data[i]));
				file_header.compressed_size = compressed_size;
				file_header.compressed_hash = ComputeHash(compressed_files_data[i], compressed_size);
				delete[] file_data;

				ZSTD_freeCCtx(cctx);
				break;
			}

			case CompressionScheme::lz4: {
				size_t compressed_size;
				if (file.getFlag(Q4B_CompressionFileFlags::DoWriteMetadata)) {
					LZ4F_preferences_t prefs = {}; // LZ4F_INIT_PREFERENCES will set to the defaults, but 0 is also interpreted as default
					prefs.compressionLevel = file.compression_level;
					// prefs.frameInfo.contentSize = file_header.uncompressed_size; // This writes 8 extra bytes

					// No need to write checksums because the archive already does that
					prefs.frameInfo.contentChecksumFlag = LZ4F_noContentChecksum; // Default already 0
					prefs.frameInfo.blockChecksumFlag = LZ4F_noBlockChecksum; // Default already 0

					compressed_size = CompressLz4Data_Metadata(&prefs, file_data, file_header.uncompressed_size, &(compressed_files_data[i]));
					file_header.setFlag(Q4B_ArchivedFileFlags::MetadataEmbedded);
				} else {
					compressed_size = CompressLz4Data(file_data, file_header.uncompressed_size, &(compressed_files_data[i]), file.compression_level);
				}

				file_header.compressed_size = compressed_size;
				file_header.compressed_hash = ComputeHash(compressed_files_data[i], compressed_size);
				delete[] file_data;
				break;
			}

			case CompressionScheme::brotli: {
				if (file.getFlag(Q4B_CompressionFileFlags::DoWriteMetadata)) {
					messages->push_back({ ErrorSeverity::warn, "Brotli doesn't support writing metadata" });
					// The blocks have a header, but Brotli doesn't have a frame format
				}
				size_t compressed_size = CompressBrotliData(file_data, file_header.uncompressed_size, &(compressed_files_data[i]), file.compression_level);
				file_header.compressed_size = compressed_size;
				file_header.compressed_hash = ComputeHash(compressed_files_data[i], compressed_size);
				delete[] file_data;
				break;
			}
		}

		if constexpr (extraFeatures) files_completed->fetch_add(1, std::memory_order_release);
	}

	if constexpr (extraFeatures)
		if (exit_flag->load(std::memory_order_acquire)) [[unlikely]] {
			messages->push_back({ ErrorSeverity::info, "Quitting early" });
			for (int i = 0; i < file_list.size(); i++) {
				delete[] compressed_files_data[i];
			}
			working_flag->store(false);
			return;
		}

	// Write archive
	ArchiveHeader ah;
	ah.num_files = file_list.size();
	ah.computeHash();
	outfile.write((const char*)&ah, sizeof(ah));

	for (const ArchivedFileHeader& fh : compressed_files_headers) {
		outfile.write((const char*)&fh, sizeof(fh));
	}
	for (int i = 0; i < file_list.size(); i++) {
		uint64_t size = compressed_files_headers[i].compressed_size;
		outfile.write((const char*)(compressed_files_data[i]), size);
	}

	outfile.close();
	std::error_code ec;
	std::filesystem::rename(output_tmp, output, ec);
	if (ec) {
		messages->push_back({ ErrorSeverity::error, "Could not convert the temp file to real file" });
		// std::filesystem::remove(output_tmp);
	}

	for (int i = 0; i < file_list.size(); i++) {
		delete[] compressed_files_data[i];
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

	char* archive;
	int64_t file_size = LoadFileIntoMemory(input, &archive);
	if (file_size == -1) {
		//TODO
	}

	ArchiveHeader ah;
	std::memcpy(&ah, archive, sizeof(ArchiveHeader));
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

	std::vector<char*> compressed_files_data(ah.num_files);
	std::vector<ArchivedFileHeader> compressed_files_headers(ah.num_files);
	size_t file_offset = sizeof(ArchiveHeader);

	for (int i = 0; i < ah.num_files; i++) {
		if (archive_size < file_offset + sizeof(ArchivedFileHeader)) {
			delete[] archive;
			std::cout << "insufficient size for headers\n";
			return;
		}

		ArchivedFileHeader& file_header = compressed_files_headers[i];
		std::memcpy(&file_header, archive + file_offset, sizeof(ArchivedFileHeader));
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
		std::memcpy(compressed_files_data[i], archive + file_offset, compressed_size);
		file_offset += compressed_size;
	}
	// std::cout << "archive size: " << archive_size << " | file offset: " << file_offset << std::endl;

	for (int i = 0; i < ah.num_files; i++) {
		auto hash = ComputeHash(compressed_files_data[i], compressed_files_headers[i].compressed_size);
		if (hash != compressed_files_headers[i].compressed_hash) {
			delete[] archive;
			for (char* f : compressed_files_data) {
				delete[] f;
			}
			// std::cout << "size: " << compressed_files_headers[i].compressed_size << " | hash stored: " << compressed_files_headers[i].compressed_hash << " | computed hash: " << hash << std::endl;
			std::cout << "hash didn't match on file " << i << std::endl;
			return;
		}
	}

	for (int i = 0; i < ah.num_files; i++) {
		const ArchivedFileHeader& file_header = compressed_files_headers[i];

		switch (file_header.compression_type) {
			default:
				std::cerr << "ERROR: Unknown compression: " << (int64_t)file_header.compression_type << std::endl;
				[[fallthrough]];
			case CompressionScheme::Uncompressed: {
				std::ofstream outfile(output.string() + "/" + std::filesystem::path(file_header.path).filename().string(), std::ios::binary);
				outfile.write((const char*)compressed_files_data[i], file_header.compressed_size);
				outfile.close();
				//TODO: should probably check hashes and size again, since the compressed_size could equal the uncompressed size on ill-formatted data
				break;
			}

			case CompressionScheme::zstd: {
				// Zstd doesn't care about the metadata, so no need to check for Q4B_ArchivedFileFlags::MetadataEmbedded
				char* decompressed_file;
				size_t decompressed_size = DecompressZstdData(compressed_files_data[i], file_header.compressed_size, &decompressed_file, file_header.uncompressed_size);
				if (decompressed_size != file_header.uncompressed_size) {
					std::cout << "file size mismatch!\n";
					//TODO
				}
				std::ofstream outfile(output.string() + "/" + std::filesystem::path(file_header.path).filename().string(), std::ios::binary);
				outfile.write((const char*)decompressed_file, decompressed_size);
				outfile.close();
				break;
			}

			case CompressionScheme::lz4: {
				char* decompressed_file;
				size_t decompressed_size;
				if (file_header.getFlag(Q4B_ArchivedFileFlags::MetadataEmbedded)) {
					decompressed_size = DecompressLz4Data_Metadata(compressed_files_data[i], file_header.compressed_size, &decompressed_file, file_header.uncompressed_size);
				} else {
					decompressed_size = DecompressLz4Data(compressed_files_data[i], file_header.compressed_size, &decompressed_file, file_header.uncompressed_size);
				}
				if (decompressed_size != file_header.uncompressed_size) {
					std::cout << "file size mismatch!\n";
					//TODO
				}
				std::ofstream outfile(output.string() + "/" + std::filesystem::path(file_header.path).filename().string(), std::ios::binary);
				outfile.write((const char*)decompressed_file, decompressed_size);
				outfile.close();
				break;
			}

			case CompressionScheme::brotli: {
				// Brotli doesn't have a frame format, so no need to check for Q4B_ArchivedFileFlags::MetadataEmbedded
				char* decompressed_file;
				size_t decompressed_size = DecompressBrotliData(compressed_files_data[i], file_header.compressed_size, &decompressed_file, file_header.uncompressed_size);
				if (decompressed_size != file_header.uncompressed_size) {
					std::cout << "file size mismatch!\n";
					//TODO
				}
				std::ofstream outfile(output.string() + "/" + std::filesystem::path(file_header.path).filename().string(), std::ios::binary);
				outfile.write((const char*)decompressed_file, decompressed_size);
				outfile.close();
				break;
			}
		}

		// std::cout << "uncompressed " << i << "\n";
	}

	std::cout << "unpacked .q4b\n";

	for (char* f : compressed_files_data) {
		delete[] f;
	}
	delete[] archive;
}

bool ReadArchiveHeader(const std::filesystem::path& input, ArchiveHeader& header, std::vector<ArchivedFileHeader>& list) noexcept {
	size_t archive_size = std::filesystem::file_size(input);
	if (archive_size < sizeof(ArchiveHeader)) {
		return false;
	}

	char* archive;
	int64_t file_size = LoadFileIntoMemory(input, &archive); //TODO: no need to load the entire file...
	if (file_size == -1) {
		//TODO
	}
	if (file_size < sizeof(ArchiveHeader)) {
		delete[] archive;
		return false;
	}
	std::memcpy(&header, archive, sizeof(ArchiveHeader));

	size_t file_offset = sizeof(ArchiveHeader);
	for (int i = 0; i < header.num_files; i++) {
		if (archive_size < file_offset + sizeof(ArchivedFileHeader)) {
			delete[] archive;
			std::cout << "insufficient size for headers\n";
			return false;
		}

		ArchivedFileHeader file_header;
		std::memcpy(&file_header, archive + file_offset, sizeof(ArchivedFileHeader));
		list.push_back(file_header);
		file_offset += sizeof(ArchivedFileHeader);
	}

	delete[] archive;
	return true;
}

int64_t LoadFileIntoMemory(const std::filesystem::path& filepath, char** dest) noexcept {
	// Open file (at the end)
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);
	if (!file) {
		return -1;
	}

	// Get file size: don't use std::filesystem::file_size because the size *could* change
	// file.seekg(0, std::ios::end); // Not needed because the file was opened at the end
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

size_t CompressZstdData(void* cctx, const void* file_data, size_t uncompressed_size, char** compressed_file) noexcept {
	size_t compressedBufSize = ZSTD_compressBound(uncompressed_size);
	*compressed_file = new char[compressedBufSize];
	size_t compressedSize = ZSTD_compress2((ZSTD_CCtx*)cctx, *compressed_file, compressedBufSize, file_data, uncompressed_size);
	return compressedSize;
}

int CompressLz4Data(const void* file_data, int uncompressedSize, char** compressed_file, int compression_level) noexcept {
	int compressedBufSize = LZ4_compressBound(uncompressedSize);
	*compressed_file = new char[compressedBufSize];
	int compressedSize = LZ4_compress_HC((const char*)file_data, *compressed_file, uncompressedSize, compressedBufSize, compression_level);
	return compressedSize;
}

size_t CompressLz4Data_Metadata(const void* prefs, const void* file_data, size_t uncompressedSize, char** compressed_file) noexcept {
	size_t compressedBufSize = LZ4F_compressFrameBound(uncompressedSize, (const LZ4F_preferences_t*)prefs);
	*compressed_file = new char[compressedBufSize];
	size_t compressedSize = LZ4F_compressFrame(*compressed_file, compressedBufSize, file_data, uncompressedSize, (const LZ4F_preferences_t*)prefs);
	// HC compression function follows LZ4's old parameter order, but frame compression follows Zstd's...
	return compressedSize;
}

size_t DecompressZstdData(const void* file_data, size_t compressed_size, char** decompressed_file, size_t decompressed_size) noexcept {
	*decompressed_file = new char[decompressed_size];
	size_t decompressedSize = ZSTD_decompress(*decompressed_file, decompressed_size, file_data, compressed_size);
	return decompressedSize;
}

int DecompressLz4Data(const void* file_data, int compressed_size, char** decompressed_file, size_t decompressed_size) noexcept {
	*decompressed_file = new char[decompressed_size];
	int decompressedSize = LZ4_decompress_safe((const char*)file_data, *decompressed_file, compressed_size, decompressed_size);
	return decompressedSize;
}

size_t DecompressLz4Data_Metadata(const void* file_data, size_t compressed_size, char** decompressed_file, size_t decompressed_size) noexcept {
	LZ4F_dctx* dctx;
	LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
	const LZ4F_decompressOptions_t dOpt = { 0, 1, 0, 0 };
	*decompressed_file = new char[decompressed_size];

	size_t srcPos = 0;
	size_t ret;
	do {
		size_t dstSize = decompressed_size;
		size_t srcSize = compressed_size - srcPos;
		ret = LZ4F_decompress(dctx, *decompressed_file, &dstSize, (char*)file_data + srcPos, &srcSize, &dOpt);
		srcPos += srcSize;
	} while (srcPos < compressed_size && ret != 0);

	LZ4F_freeDecompressionContext(dctx);
	return decompressed_size; // TODO
}

size_t CompressBrotliData(const void* file_data, size_t uncompressed_size, char** compressed_file, int compression_level) noexcept {
	size_t compressedBufSize = BrotliEncoderMaxCompressedSize(uncompressed_size); //TODO: what to do when compression level not >=2?
	*compressed_file = new char[compressedBufSize];
	int ret = BrotliEncoderCompress(compression_level, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, uncompressed_size, (const uint8_t*) file_data, &compressedBufSize, (uint8_t*) *compressed_file);
	// CLI default lgwin is 24, encode.h default is 22; why do these compression formats always make their CLI different?
	return compressedBufSize; // Brotli takes the buffer size as an input and changes it to the compressed size
}

size_t DecompressBrotliData(const void* file_data, size_t compressed_size, char** decompressed_file, size_t decompressed_size) noexcept {
	*decompressed_file = new char[decompressed_size];
	size_t decompressedSize = decompressed_size;
	BrotliDecoderResult ret = BrotliDecoderDecompress(compressed_size, (const uint8_t*) file_data, &decompressedSize, (uint8_t*) *decompressed_file);
	return decompressedSize;
}

} // namespace q4b
