#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../q4b.hpp"

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
	std::vector<int> clevel_val;
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

#ifdef Q4B_ENABLE_LZ4
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
#endif

#ifdef Q4B_ENABLE_ZSTD
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
#endif

#ifdef Q4B_ENABLE_BROTLI
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
#endif

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

#ifdef Q4B_ENABLE_STB
struct CompressionSchemeData_Stb final : public CompressionSchemeData {
	CompressionSchemeData_Stb() {
		scheme = q4b::CompressionScheme::stb;
		displayName = "stb";
		internalName = "stb";
		searchNames = { "stb", "STB", "stb_compress" };
		isDictionaryScheme = false;

		recommendation = CompressionSchemeRecommendedLevel::Awful;
		usableForGenericExport = false;
		supportsMetadata = false;
		information_text = "A basic LZ77 compression scheme. Unmaintained by STB.\n"
		                   "Only notably used by Dear Imgui its embedded fonts, because the compressor/decompressor is small.";
		// As stb.h is poorly documented, the information was gathered from: https://github.com/jrk/stb/blob/master/stb_compress.txt
	}
	void Initialize() noexcept override;
};
#endif
