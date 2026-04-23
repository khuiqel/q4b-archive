#include "q4b.hpp"

#include <fstream>
#include <iostream>
#include <cstring> //memcpy, strncpy?

#include <zstd.h>
#include <lz4.h>
#include <lz4frame.h>

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



void ExistencePrune(std::vector<CompressionFile>& file_list) noexcept {
	auto it = std::remove_if(file_list.begin(), file_list.end(),
		[](const auto& file) {
			std::error_code ec;
			return !std::filesystem::exists(file.filepath, ec);
		}
	);
	file_list.erase(it, file_list.end());
}
//probably put this on a different thread

std::vector<CompressionFile> GetFileListSortedBySize(const std::vector<CompressionFile>& file_list) noexcept {
	std::vector<CompressionFile> sorted_files(file_list.begin(), file_list.end());
	std::sort(sorted_files.begin(), sorted_files.end(), [](const auto& lhs, const auto& rhs) {
		std::error_code ec;
		return std::filesystem::file_size(lhs.filepath, ec) < std::filesystem::file_size(rhs.filepath, ec);
	});
	return sorted_files;
}



//TODO: should this make sure there are no duplicates?
void WriteArchive(const std::vector<CompressionFile>& file_list, const std::filesystem::path& output) noexcept {
	const std::filesystem::path output_tmp = output.string() + ".tmp";
	std::ofstream outfile(output_tmp, std::ios::binary);
	if (!outfile) {
		return;
	}

	std::vector<char*> compressed_files_data(file_list.size());
	std::vector<ArchivedFileHeader> compressed_files_headers(file_list.size());

	for (int i = 0; i < file_list.size(); i++) {
		const CompressionFile& file = file_list[i];
		char* file_data;
		int64_t file_size = LoadFileIntoMemory(file.filepath, &file_data);
		if (file_size == -1) {
			//TODO
		}

		ArchivedFileHeader& file_header = compressed_files_headers[i];
		strncpy(file_header.path, file.getFilepath(), sizeof(file_header.path)-1);
		std::replace(file_header.path, file_header.path + sizeof(file_header.path), '\\', '/');
		file_header.compression_type = file.compression_type;
		file_header.uncompressed_size = file_size;
		file_header.uncompressed_hash = ComputeHash(file_data, file_header.uncompressed_size);

		switch (file.compression_type) {
			default:
				std::cerr << "ERROR: Unknown compression: " << (int64_t)file.compression_type << std::endl;
				file_header.compression_type = CompressionScheme::Uncompressed;
				[[fallthrough]];
			case CompressionScheme::Uncompressed: {
				compressed_files_data[i] = file_data;
				file_header.compressed_size = file_header.uncompressed_size;
				file_header.compressed_hash = file_header.uncompressed_hash;
				break;
			}

			case CompressionScheme::zstd: {
				size_t compressed_size = CompressZstdData(file_data, file_header.uncompressed_size, file.compression_level, &(compressed_files_data[i]));
				file_header.compressed_size = compressed_size;
				file_header.compressed_hash = ComputeHash(compressed_files_data[i], compressed_size);
				// std::cout << "size: " << file_header.compressed_size << " | hash: " << file_header.compressed_hash << std::endl;
				delete[] file_data;
				break;
			}

			case CompressionScheme::lz4: {
				size_t compressed_size = CompressLz4Data(file_data, file_header.uncompressed_size, file.compression_level, &(compressed_files_data[i]));
				file_header.compressed_size = compressed_size;
				file_header.compressed_hash = ComputeHash(compressed_files_data[i], compressed_size);
				delete[] file_data;
				break;
			}
		}

		// std::cout << "compressed " << i << "\n";
	}

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
		std::filesystem::remove(output_tmp);
	} else {
		// std::cout << "wrote .q4b\n";
	}

	for (int i = 0; i < file_list.size(); i++) {
		delete[] compressed_files_data[i];
	}
}

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
				char* decompressed_file;
				size_t decompressed_size = UncompressZstdData(compressed_files_data[i], file_header.compressed_size, &decompressed_file, file_header.uncompressed_size);
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
				size_t decompressed_size = UncompressLz4Data(compressed_files_data[i], file_header.compressed_size, &decompressed_file, file_header.uncompressed_size);
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

//TODO: compressing without making the frame needs a context
size_t CompressZstdData(const void* file_data, size_t uncompressedSize, int compression_level, char** compressed_file) noexcept {
	size_t compressedBufSize = ZSTD_compressBound(uncompressedSize);
	*compressed_file =  new char[compressedBufSize];
	size_t compressedSize = ZSTD_compress(*compressed_file, compressedBufSize, file_data, uncompressedSize, compression_level);
	return compressedSize;
	//TODO: handle --max, also should probably have a fixed Zstd context
	//TODO: ZSTD_getErrorName()
}

size_t CompressLz4Data(const void* file_data, size_t uncompressedSize, int compression_level, char** compressed_file) noexcept {
	(void)compression_level; //TODO
	size_t compressedBufSize = LZ4_compressBound(uncompressedSize);
	*compressed_file =  new char[compressedBufSize];
	size_t compressedSize = LZ4_compress_default((const char*)file_data, (char*)(*compressed_file), uncompressedSize, compressedBufSize);
	return compressedSize;
}

size_t UncompressZstdData(const void* file_data, size_t compressed_file_size, char** decompressed_file, size_t decompressed_file_size) noexcept {
	// size_t decompressedBufSize = ZSTD_getFrameContentSize(file_data, compressed_file_size); // ZSTD_decompressBound()
	*decompressed_file = new char[decompressed_file_size];
	size_t decompressedSize = ZSTD_decompress(*decompressed_file, decompressed_file_size, file_data, compressed_file_size);
	return decompressedSize;
}

size_t UncompressLz4Data(const void* file_data, size_t compressed_file_size, char** decompressed_file, size_t decompressed_file_size) noexcept {
	// size_t decompressedBufSize = ZSTD_getFrameContentSize(file_data, compressed_file_size); // (LZ4 generates blocks by default, not frames)
	*decompressed_file = new char[decompressed_file_size];
	size_t decompressedSize = LZ4_decompress_safe((const char*)file_data, (char*)(*decompressed_file), compressed_file_size, decompressed_file_size);
	return decompressedSize;
}

} // namespace q4b
