#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "q4b.hpp"

enum class CompressionSchemeRecommendedLevel : uint8_t {
	No_Opinion,
	Awful,
	Okay,
	Good,
	Best,
};

// Data to be displayed by the GUI
struct CompressionSchemeData {
	q4b::CompressionScheme scheme;
	const char* displayName;
	const char* internalName;
	std::vector<std::string> searchNames;
	bool isDictionaryScheme;

	CompressionSchemeRecommendedLevel recommendation;
	bool usableForGenericExport; // Must be able to export frames, and have programs that use the frames
	bool supportsMetadata; //TODO: is this different?
	const char* information_text;

	std::vector<char*> clevel_str;
	std::vector<int> clevel_num;
	int clevel_default_idx;

	virtual void Initialize() noexcept = 0;
	virtual ~CompressionSchemeData() {
		for (char* str : clevel_str) {
			delete[] str;
		}
	}
	//TODO: constructor calls Initialize()?
};

struct CompressionSchemeData_Uncompressed final : public CompressionSchemeData {
	CompressionSchemeData_Uncompressed() {
		scheme = q4b::CompressionScheme::Uncompressed;
		displayName = "Uncompressed"; //TODO: q4b::CompressionToStr()
		internalName = "uncompressed";
		searchNames = { "Uncompressed", "uncompressed", "none", "" };
		isDictionaryScheme = false;

		recommendation = CompressionSchemeRecommendedLevel::No_Opinion;
		usableForGenericExport = false; // Technically yes, but don't bother officially supporting because why
		supportsMetadata = false;
		information_text = "No compression scheme.";
	}
	void Initialize() noexcept override;
};

struct CompressionSchemeData_Lz4 final : public CompressionSchemeData {
	CompressionSchemeData_Lz4() {
		scheme = q4b::CompressionScheme::lz4;
		displayName = "LZ4";
		internalName = "lz4";
		searchNames = { "LZ4", "lz4" };
		isDictionaryScheme = false;

		recommendation = CompressionSchemeRecommendedLevel::Best;
		usableForGenericExport = true;
		supportsMetadata = true;
		information_text = "Decent compression ratio, great compression speed, and EXTREMELY fast decompression speed.\n\n"
		                   "Note that this program uses LZ4HC when not adding metadata and LZ4F when adding metadata. They produce slightly different results.";
	}
	void Initialize() noexcept override;
};

struct CompressionSchemeData_Zstd final : public CompressionSchemeData {
	CompressionSchemeData_Zstd() {
		scheme = q4b::CompressionScheme::zstd;
		displayName = "Zstd";
		internalName = "zstd";
		searchNames = { "Zstd", "zstd", "ZSTD" };
		isDictionaryScheme = false;

		recommendation = CompressionSchemeRecommendedLevel::Best;
		usableForGenericExport = true;
		supportsMetadata = true;
		information_text = "Great compression ratio and great decompression speed. Good compression speed.\n"
		                   "Created by Yann Collet, who also created LZ4 and is a core maintainer of OpenZL.";
	}
	void Initialize() noexcept override;
	//TODO: store the cctx
};

/*
struct CompressionSchemeData_Zstd_Dict final : public CompressionSchemeData {
	CompressionSchemeData_Zstd_Dict() {
		scheme = q4b::CompressionScheme::zstd_dict;
		displayName = "Zstd (dictionary)";
		internalName = "zstd_dict";
		searchNames = { "Zstd (dictionary)", "zstd_dict", "ZSTD_DICT" };
		isDictionaryScheme = true;

		recommendation = CompressionSchemeRecommendedLevel::Best;
		usableForGenericExport = true; // TODO
		supportsMetadata = true; // TODO
		information_text = "Zstd dictionary compression. Possibly the best general-purpose dictionary compressor.";
	}
	void Initialize() noexcept override;
};
*/

struct CompressionSchemeData_Brotli final : public CompressionSchemeData {
	CompressionSchemeData_Brotli() {
		scheme = q4b::CompressionScheme::brotli;
		displayName = "Brotli";
		internalName = "brotli";
		searchNames = { "Brotli", "brotli" };
		isDictionaryScheme = false;

		recommendation = CompressionSchemeRecommendedLevel::Good;
		usableForGenericExport = false;
		supportsMetadata = false;
		information_text = "A bit faster than single-threaded Zstd while having a bit worse compression ratio. Successor to gzip. Designed for font files and text.";
	}
	void Initialize() noexcept override;
};

/*
struct CompressionSchemeData_Zlib final : public CompressionSchemeData {
	CompressionSchemeData_Zlib() {
		scheme = q4b::CompressionScheme::zlib;
		displayName = "zlib";
		internalName = "zlib";
		searchNames = { "zlib", "zlib-ng", "miniz" };
		isDictionaryScheme = false;

		recommendation = CompressionSchemeRecommendedLevel::Okay;
		usableForGenericExport = false; // TODO
		supportsMetadata = false; //TODO
		information_text = "An old compression scheme. There are better options. Not to be confused with Zstd.\n\n"
		                   "This particular program implements the zlib data format using miniz.";
	}
	void Initialize() noexcept override;
};
*/



struct CompressionSchemeFunctions {
	[[nodiscard]] virtual uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept = 0;
	virtual uint64_t GetMaxSize() const = 0;
	virtual ~CompressionSchemeFunctions() = default;
};

struct CompressionSchemeFunctions_Lz4 final : public CompressionSchemeFunctions {
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	uint64_t GetMaxSize() const override;
	~CompressionSchemeFunctions_Lz4();
};

struct CompressionSchemeFunctions_Zstd final : public CompressionSchemeFunctions {
	//TODO: store cctx
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	uint64_t GetMaxSize() const override;
	~CompressionSchemeFunctions_Zstd();
};

struct CompressionSchemeFunctions_Brotli final : public CompressionSchemeFunctions {
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	uint64_t GetMaxSize() const override;
	~CompressionSchemeFunctions_Brotli();
};
