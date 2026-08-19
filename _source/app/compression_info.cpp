#include "compression_info.hpp"
#include <climits> //INT_MAX

void CompressionSchemeData_Uncompressed::Initialize() noexcept {
	char* str = new char[2];
	str[0] = '0'; str[1] = '\0';
	clevel_str.push_back(str);
	clevel_val.push_back(0);
	clevel_default_idx = 0;
}

#ifdef Q4B_ENABLE_LZ4
#include <lz4frame.h>
void CompressionSchemeData_Lz4::Initialize() noexcept {
	clevel_str.reserve(LZ4F_compressionLevel_max());
	clevel_val.reserve(LZ4F_compressionLevel_max());

	for (int i = 1; i <= LZ4F_compressionLevel_max(); i++) {
		std::string level = std::to_string(i);
		if (i == 1) { // LZ4_CLEVEL_DEFAULT
			level += " (CLI default)";
			clevel_default_idx = clevel_val.size();
			// Note: level 1 and 2 are identical, see k_clTable in lz4hc.c
		} else if (i == 9) { // LZ4HC_CLEVEL_DEFAULT
			level += " (HC default)";
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		clevel_str.push_back(level_str);
		clevel_val.push_back(i);
	}
}
#endif

#ifdef Q4B_ENABLE_ZSTD
#include <zstd.h>
void CompressionSchemeData_Zstd::Initialize() noexcept {
	clevel_str.reserve(ZSTD_maxCLevel()+1);
	clevel_val.reserve(ZSTD_maxCLevel()+1);

	for (int i = 1; i <= ZSTD_maxCLevel(); i++) {
		std::string level = std::to_string(i);
		if (i > 19) { // ZSTDCLI_CLEVEL_MAX
			level += " --ultra";
			// CLI disables values >19, but the zstd lib doesn't care
			// Values needing --ultra tend to disable/reduce multi-threading though
		}
		if (i == ZSTD_defaultCLevel()) {
			level += " (default)";
			clevel_default_idx = clevel_val.size();
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		clevel_str.push_back(level_str);
		clevel_val.push_back(i);
	}

	const char* level = "--max (NOT RECOMMENDED)";
	char* level_str = new char[sizeof("--max (NOT RECOMMENDED)")+1];
	std::copy(level, level+sizeof("--max (NOT RECOMMENDED)"), level_str);
	clevel_str.push_back(level_str);
	clevel_val.push_back(INT_MAX);
}
#endif

#ifdef Q4B_ENABLE_BROTLI
#include <brotli/encode.h>
void CompressionSchemeData_Brotli::Initialize() noexcept {
	clevel_str.reserve((BROTLI_MAX_QUALITY - BROTLI_MIN_QUALITY) + 1);
	clevel_val.reserve((BROTLI_MAX_QUALITY - BROTLI_MIN_QUALITY) + 1);

	for (int i = BROTLI_MIN_QUALITY; i <= BROTLI_MAX_QUALITY; i++) {
		std::string level = std::to_string(i);
		if (i == BROTLI_MAX_QUALITY) {
			level += " --best";
		}
		if (i == BROTLI_DEFAULT_QUALITY) {
			// Default quality is highest quality, so no need to split these conditionals
			level += " (default)";
			clevel_default_idx = clevel_val.size();
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		clevel_str.push_back(level_str);
		clevel_val.push_back(i);
	}
}
#endif

#ifdef Q4B_ENABLE_STB
void CompressionSchemeData_Stb::Initialize() noexcept {
	char* str = new char[2];
	str[0] = '0'; str[1] = '\0';
	clevel_str.push_back(str);
	clevel_val.push_back(0);
	clevel_default_idx = 0;
}
#endif
