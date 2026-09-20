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

struct CompressionSchemeInfo {
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
	virtual void Initialize_Gui() = 0;
	virtual ~CompressionSchemeInfo() {
		// GUI
		if (clevel_str.size() > 0) {
			for (char* str : clevel_str) {
				delete[] str;
			}
		}
	}
};

struct CompressionSchemeInfo_Uncompressed final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Uncompressed() {
		scheme = q4b::CompressionScheme::Uncompressed;
		isDictionaryScheme = false;
		usableForGenericExport = false; // Technically yes, but don't bother officially supporting because why

		recommendation = CompressionSchemeRecommendedLevel::No_Opinion;
		displayName = "Uncompressed";
		informationText = "No compression scheme.";

		helpText = "TODO";
		searchNames = { "Uncompressed", "uncompressed", "none", "" };
		fileExtensions = { ".uncompressed" };
	}
	void Initialize_Gui() override;
};

#ifdef Q4B_ENABLE_LZ4
struct CompressionSchemeInfo_Lz4 final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Lz4() {
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
	void Initialize_Gui() override;
};
#endif

#ifdef Q4B_ENABLE_ZSTD
struct CompressionSchemeInfo_Zstd final : public CompressionSchemeInfo {
	//TODO: store cctx
	CompressionSchemeInfo_Zstd() {
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
	void Initialize_Gui() override;
};

/*
struct CompressionSchemeInfo_Zstd_Dict final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Zstd_Dict() {
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
	void Initialize_Gui() override;
};
*/
#endif

#ifdef Q4B_ENABLE_BROTLI
struct CompressionSchemeInfo_Brotli final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Brotli() {
		scheme = q4b::CompressionScheme::brotli;
		isDictionaryScheme = false;
		usableForGenericExport = false;

		recommendation = CompressionSchemeRecommendedLevel::Good;
		displayName = "Brotli";
		informationText = "A bit faster than single-threaded Zstd while having a bit worse compression ratio. Successor to gzip (and Zopfli?). Designed for font files and HTML text.";

		helpText = "clevels: 0-11";
		searchNames = { "Brotli", "brotli" };
		fileExtensions = { ".br" };
	}
	void Initialize_Gui() override;
};
#endif

#ifdef Q4B_ENABLE_SNAPPY
struct CompressionSchemeInfo_Snappy final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Snappy() {
		scheme = q4b::CompressionScheme::snappy;
		isDictionaryScheme = false;
		usableForGenericExport = true; // Snappy does have a frame format, but doesn't implement it. Fortunately streams start with the uncompressed length

		recommendation = CompressionSchemeRecommendedLevel::Okay;
		displayName = "Snappy";
		informationText = "Focuses on simplicity while being fast. About equal to LZ4 level 1 for compression speed and ratio.\n"
		                  "Used by Google and has compressed many petabytes. Really its main selling point is simplicity.";

		helpText = "clevels: 1-2"; // 2 is experimental, 3-9 not supported
		searchNames = { "Snappy", "snappy", "Zippy", "zippy" };
		fileExtensions = { ".sz" };
	}
	void Initialize_Gui() override;
};
#endif

/*
struct CompressionSchemeInfo_Zlib final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Zlib() {
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
	void Initialize_Gui() override;
};
*/

#ifdef Q4B_ENABLE_STB
struct CompressionSchemeInfo_Stb final : public CompressionSchemeInfo {
	CompressionSchemeInfo_Stb() {
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
	void Initialize_Gui() override;
};
#endif
