#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../q4b.hpp"

enum class CompressionSchemeRecommendedLevel : uint8_t {
	No_Opinion,
	Awful,
	Okay,
	Good,
	Best,
};

struct CompressionSchemeData {
	// Core info
	q4b::CompressionScheme scheme;
	bool isDictionaryScheme;
	bool usableForGenericExport; // Must be able to export frames, and have programs that use the frames

	// GUI data
	CompressionSchemeRecommendedLevel recommendation;
	const char* displayName;
	const char* informationText;

	std::vector<char*> clevel_str;
	std::vector<int> clevel_val;
	int clevel_default_idx;

	// CLI data
	const char* helpText; // More detailed than informationText
	std::vector<std::string> searchNames;
	std::vector<std::string> fileExtensions; // Has the '.'

	// Rest
	virtual void Initialize() noexcept = 0; //TODO: Initialize_Gui()?
	virtual ~CompressionSchemeData() {
		// GUI
		for (char* str : clevel_str) {
			delete[] str;
		}
	}
};

struct CompressionSchemeData_Uncompressed final : public CompressionSchemeData {
	CompressionSchemeData_Uncompressed() {
		scheme = q4b::CompressionScheme::Uncompressed;
		isDictionaryScheme = false;
		usableForGenericExport = false; // Technically yes, but don't bother officially supporting because why

		recommendation = CompressionSchemeRecommendedLevel::No_Opinion;
		displayName = "Uncompressed"; //TODO: q4b::CompressionToStr()
		informationText = "No compression scheme.";

		helpText = "TODO";
		searchNames = { "Uncompressed", "uncompressed", "none", "" };
		fileExtensions = { ".uncompressed" };
	}
	void Initialize() noexcept override;
};

#ifdef Q4B_ENABLE_LZ4
struct CompressionSchemeData_Lz4 final : public CompressionSchemeData {
	CompressionSchemeData_Lz4() {
		scheme = q4b::CompressionScheme::lz4;
		isDictionaryScheme = false;
		usableForGenericExport = true;

		recommendation = CompressionSchemeRecommendedLevel::Best;
		displayName = "LZ4";
		informationText = "Decent compression ratio, great compression speed, and EXTREMELY fast decompression speed.\n\n"
		                  "Note that this program uses LZ4F for generic exports and LZ4HC for Q4B archives. They produce slightly different results.";

		helpText = "clevels: 1-12";
		searchNames = { "LZ4", "lz4" };
		fileExtensions = { ".lz4" };
	}
	void Initialize() noexcept override;
};
#endif

#ifdef Q4B_ENABLE_ZSTD
struct CompressionSchemeData_Zstd final : public CompressionSchemeData {
	//TODO: store cctx
	CompressionSchemeData_Zstd() {
		scheme = q4b::CompressionScheme::zstd;
		isDictionaryScheme = false;
		usableForGenericExport = true;

		recommendation = CompressionSchemeRecommendedLevel::Best;
		displayName = "Zstd";
		informationText = "Great compression ratio and great decompression speed. Good compression speed.\n"
		                  "Created by Yann Collet, who also created LZ4 and is a core maintainer of OpenZL.";

		helpText = "clevels: 1-22 & max";
		searchNames = { "Zstd", "zstd", "ZSTD", "Zstandard" };
		fileExtensions = { ".zst" };
	}
	void Initialize() noexcept override;
};

/*
struct CompressionSchemeData_Zstd_Dict final : public CompressionSchemeData {
	CompressionSchemeData_Zstd_Dict() {
		scheme = q4b::CompressionScheme::zstd_dict;
		isDictionaryScheme = true;
		usableForGenericExport = true; // TODO

		recommendation = CompressionSchemeRecommendedLevel::Best;
		displayName = "Zstd (dictionary)";
		informationText = "Zstd dictionary compression. Possibly the best general-purpose dictionary compressor.";

		helpText = "TODO";
		searchNames = { "Zstd (dictionary)", "zstd_dict", "ZSTD_DICT" };
		fileExtensions = {}; //TODO
	}
	void Initialize() noexcept override;
};
*/
#endif

#ifdef Q4B_ENABLE_BROTLI
struct CompressionSchemeData_Brotli final : public CompressionSchemeData {
	CompressionSchemeData_Brotli() {
		scheme = q4b::CompressionScheme::brotli;
		isDictionaryScheme = false;
		usableForGenericExport = false;

		recommendation = CompressionSchemeRecommendedLevel::Good;
		displayName = "Brotli";
		informationText = "A bit faster than single-threaded Zstd while having a bit worse compression ratio. Successor to gzip. Designed for font files and HTML text.";

		helpText = "clevels: 0-11";
		searchNames = { "Brotli", "brotli" };
		fileExtensions = { ".br" };
	}
	void Initialize() noexcept override;
};
#endif

/*
struct CompressionSchemeData_Zlib final : public CompressionSchemeData {
	CompressionSchemeData_Zlib() {
		scheme = q4b::CompressionScheme::zlib;
		isDictionaryScheme = false;
		usableForGenericExport = false; // TODO

		recommendation = CompressionSchemeRecommendedLevel::Okay;
		displayName = "zlib";
		informationText = "An old compression scheme. There are better options. Not to be confused with Zstd.\n\n"
		                  "This particular program implements the zlib data format using miniz.";

		helpText = "TODO";
		searchNames = { "zlib", "zlib-ng", "miniz" };
		fileExtensions = {}; //TODO
	}
	void Initialize() noexcept override;
};
*/

#ifdef Q4B_ENABLE_STB
struct CompressionSchemeData_Stb final : public CompressionSchemeData {
	CompressionSchemeData_Stb() {
		scheme = q4b::CompressionScheme::stb;
		isDictionaryScheme = false;
		usableForGenericExport = false;

		recommendation = CompressionSchemeRecommendedLevel::Awful;
		displayName = "stb";
		informationText = "A basic LZ77 compression scheme. Unmaintained by STB.\n"
		                  "Only notably used by Dear Imgui for its embedded fonts, because the compressor/decompressor is small.";
		// As stb.h is poorly documented, the information was gathered from: https://github.com/jrk/stb/blob/master/stb_compress.txt

		helpText = "clevels: none";
		searchNames = { "stb", "STB", "stb_compress" };
		fileExtensions = { ".stb" };
	}
	void Initialize() noexcept override;
};
#endif
