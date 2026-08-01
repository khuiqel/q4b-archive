#include "compression_schemes.hpp"
#include <iostream>

void CompressionSchemeData_Uncompressed::Initialize() noexcept {
	char* str = new char[2];
	str[0] = '0'; str[1] = '\0';
	clevel_str.push_back(str);
	clevel_num.push_back(0);
	clevel_default_idx = 0;
}

#ifdef Q4B_ENABLE_LZ4
#include <lz4frame.h>
void CompressionSchemeData_Lz4::Initialize() noexcept {
	clevel_str.reserve(LZ4F_compressionLevel_max());
	clevel_num.reserve(LZ4F_compressionLevel_max());

	for (int i = 1; i <= LZ4F_compressionLevel_max(); i++) {
		std::string level = std::to_string(i);
		if (i == 1) { // LZ4_CLEVEL_DEFAULT
			level += " (CLI default)";
			clevel_default_idx = clevel_num.size();
			// Note: level 1 and 2 are identical, see k_clTable in lz4hc.c
		} else if (i == 9) { // LZ4HC_CLEVEL_DEFAULT
			level += " (HC default)";
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		clevel_str.push_back(level_str);
		clevel_num.push_back(i);
	}
}
#endif

#ifdef Q4B_ENABLE_ZSTD
#include <zstd.h>
void CompressionSchemeData_Zstd::Initialize() noexcept {
	clevel_str.reserve(ZSTD_maxCLevel()+1);
	clevel_num.reserve(ZSTD_maxCLevel()+1);

	for (int i = 1; i <= ZSTD_maxCLevel(); i++) {
		std::string level = std::to_string(i);
		if (i > 19) { // ZSTDCLI_CLEVEL_MAX
			level += " --ultra";
			// CLI disables values >19, but the zstd lib doesn't care
			// Values needing --ultra tend to disable/reduce multi-threading though
		}
		if (i == ZSTD_defaultCLevel()) {
			level += " (default)";
			clevel_default_idx = clevel_num.size();
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		clevel_str.push_back(level_str);
		clevel_num.push_back(i);
	}

	const char* level = "--max (NOT RECOMMENDED)";
	char* level_str = new char[sizeof("--max (NOT RECOMMENDED)")+1];
	std::copy(level, level+sizeof("--max (NOT RECOMMENDED)"), level_str);
	clevel_str.push_back(level_str);
	clevel_num.push_back(INT_MAX);
}

/*
void CompressionSchemeData_Zstd_Dict::Initialize() noexcept {
	//TODO
}
*/
#endif

#ifdef Q4B_ENABLE_BROTLI
#include <brotli/encode.h>
void CompressionSchemeData_Brotli::Initialize() noexcept {
	clevel_str.reserve((BROTLI_MAX_QUALITY - BROTLI_MIN_QUALITY) + 1);
	clevel_num.reserve((BROTLI_MAX_QUALITY - BROTLI_MIN_QUALITY) + 1);

	for (int i = BROTLI_MIN_QUALITY; i <= BROTLI_MAX_QUALITY; i++) {
		std::string level = std::to_string(i);
		if (i == BROTLI_MAX_QUALITY) {
			level += " --best";
		}
		if (i == BROTLI_DEFAULT_QUALITY) {
			// Default quality is highest quality, so no need to split these conditionals
			level += " (default)";
			clevel_default_idx = clevel_num.size();
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		clevel_str.push_back(level_str);
		clevel_num.push_back(i);
	}
}
#endif

/*
void CompressionSchemeData_Zlib::Initialize() noexcept {
	//TODO
}
*/



#ifdef Q4B_ENABLE_LZ4
#include <lz4hc.h>
#include <lz4frame.h>

uint64_t CompressionSchemeFunctions_Lz4::GetMaxSize() const {
	//TODO
	return LZ4_MAX_INPUT_SIZE;
}

uint64_t CompressionSchemeFunctions_Lz4::Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept {
	uint64_t compressedSize;
	if (flags & q4b::Q4B_CompressionFileFlags::DoWriteMetadata) {
		// Setup
		LZ4F_preferences_t prefs = {}; // LZ4F_INIT_PREFERENCES will set to the defaults, but 0 is also interpreted as default
		prefs.compressionLevel = clevel;
		// prefs.frameInfo.contentSize = uncompressedSize; // This writes 8 extra bytes

		// No need to write checksums because the archive already does that
		prefs.frameInfo.contentChecksumFlag = LZ4F_noContentChecksum; // Default already 0
		prefs.frameInfo.blockChecksumFlag = LZ4F_noBlockChecksum; // Default already 0

		// Compress
		uint64_t compressedBufSize = LZ4F_compressFrameBound(uncompressedSize, &prefs);
		*outputData = new char[compressedBufSize];
		compressedSize = LZ4F_compressFrame(*outputData, compressedBufSize, inputData, uncompressedSize, &prefs);
		// HC compression function follows LZ4's old parameter order, but frame compression follows Zstd's...
	} else {
		// Compress
		//TODO: loop LZ4_MAX_INPUT_SIZE at a time
		int compressedBufSize = LZ4_compressBound(uncompressedSize);
		*outputData = new char[compressedBufSize];
		compressedSize = LZ4_compress_HC((const char*)inputData, (char*)(*outputData), uncompressedSize, compressedBufSize, clevel);
	}

	return compressedSize;
}

uint64_t CompressionSchemeFunctions_Lz4::Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept {
	uint64_t compressedSize;
	if (flags & q4b::Q4B_CompressionFileFlags::DoWriteMetadata) {
		// Setup
		LZ4F_preferences_t prefs = {};
		prefs.compressionLevel = clevel;
		prefs.frameInfo.contentSize = uncompressedSize;

		// Supposed to add checksums
		prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
		prefs.frameInfo.blockChecksumFlag = LZ4F_blockChecksumEnabled;

		// Compress
		uint64_t compressedBufSize = LZ4F_compressFrameBound(uncompressedSize, &prefs);
		*outputData = new char[compressedBufSize];
		compressedSize = LZ4F_compressFrame(*outputData, compressedBufSize, inputData, uncompressedSize, &prefs);
	} else {
		// Compress (TODO)
		//TODO: loop LZ4_MAX_INPUT_SIZE at a time
		int compressedBufSize = LZ4_compressBound(uncompressedSize);
		*outputData = new char[compressedBufSize];
		compressedSize = LZ4_compress_HC((const char*)inputData, (char*)(*outputData), uncompressedSize, compressedBufSize, clevel);
	}

	return compressedSize;
}

uint64_t CompressionSchemeFunctions_Lz4::Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept {
	//TODO
	*outputData = new char[originalSize];
	int decompressedSize = LZ4_decompress_safe((const char*)inputData, (char*)(*outputData), compressedSize, originalSize);
	return decompressedSize;
}

uint64_t CompressionSchemeFunctions_Lz4::Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept {
	LZ4F_dctx* dctx;
	LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
	const LZ4F_decompressOptions_t dOpt = { 0, 1, 0, 0 };

	LZ4F_frameInfo_t frameInfo;
	size_t unused;
	LZ4F_getFrameInfo(dctx, &frameInfo, inputData, &unused);
	uint64_t decompressedBufSize = frameInfo.contentSize;
	if (frameInfo.contentSize == 0) {
		//https://stackoverflow.com/questions/25740471/lz4-library-decompressed-data-upper-bound-size-estimation#25755758
		decompressedBufSize = 24 + 255 * (compressedSize - 10);
	}
	*outputData = new char[decompressedBufSize];

	size_t srcPos = 0;
	size_t ret;
	do {
		size_t dstSize = decompressedBufSize;
		size_t srcSize = compressedSize - srcPos;
		ret = LZ4F_decompress(dctx, *outputData, &dstSize, (char*)inputData + srcPos, &srcSize, &dOpt);
		srcPos += srcSize;
	} while (srcPos < compressedSize && ret != 0);

	LZ4F_freeDecompressionContext(dctx);
	return srcPos;
}

CompressionSchemeFunctions_Lz4::~CompressionSchemeFunctions_Lz4() {
	//TODO: free cctx
}
#endif

#ifdef Q4B_ENABLE_ZSTD
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

uint64_t CompressionSchemeFunctions_Zstd::GetMaxSize() const {
	return INT64_MAX;
}

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

uint64_t CompressionSchemeFunctions_Zstd::Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept {
	ZSTD_CCtx* cctx = ZSTD_createCCtx();
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, 1); //TODO
	if (clevel == INT_MAX) [[unlikely]] {
		zstd_setMaxCompression(cctx);
	} else {
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, clevel);
		// Other parameters are automatically set given the compression level (that would've been a huge headache otherwise)
	}

	if (flags & q4b::Q4B_CompressionFileFlags::DoWriteMetadata) {
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 1); // Default already 1
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_dictIDFlag, 0); // Not necessary
	} else {
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 0); // Default already 0
		ZSTD_CCtx_setParameter(cctx, ZSTD_c_dictIDFlag, 0); // Not necessary
	}

	uint64_t compressedBufSize = ZSTD_compressBound(uncompressedSize);
	*outputData = new char[compressedBufSize];
	uint64_t compressedSize = ZSTD_compress2((ZSTD_CCtx*)cctx, *outputData, compressedBufSize, inputData, uncompressedSize);
	ZSTD_freeCCtx(cctx);
	return compressedSize;
}

uint64_t CompressionSchemeFunctions_Zstd::Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept {
	//TODO
	return 0;
}

uint64_t CompressionSchemeFunctions_Zstd::Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept {
	*outputData = new char[originalSize];
	uint64_t decompressedSize = ZSTD_decompress(*outputData, originalSize, inputData, compressedSize);
	return decompressedSize;
}

uint64_t CompressionSchemeFunctions_Zstd::Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept {
	uint64_t decompressedBufSize = ZSTD_getFrameContentSize(inputData, compressedSize);
	if (decompressedBufSize == ZSTD_CONTENTSIZE_UNKNOWN || decompressedBufSize == ZSTD_CONTENTSIZE_ERROR) {
		decompressedBufSize = ZSTD_decompressBound(inputData, compressedSize);
		if (decompressedBufSize == ZSTD_CONTENTSIZE_ERROR) {
			std::cout << "could not determine bound of data\n";
			decompressedBufSize = 1 << 30;
		}
	}
	*outputData = new char[decompressedBufSize];
	uint64_t decompressedSize = ZSTD_decompress(*outputData, decompressedBufSize, inputData, compressedSize);
	return decompressedSize;
}

CompressionSchemeFunctions_Zstd::~CompressionSchemeFunctions_Zstd() {
	//TODO: free cctx
}
#endif

#ifdef Q4B_ENABLE_BROTLI
#include <brotli/encode.h>
#include <brotli/decode.h>

uint64_t CompressionSchemeFunctions_Brotli::GetMaxSize() const {
	return INT64_MAX;
}

uint64_t CompressionSchemeFunctions_Brotli::Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept {
	size_t compressedBufSize = BrotliEncoderMaxCompressedSize(uncompressedSize); //TODO: what to do when compression level not >=2?
	*outputData = new char[compressedBufSize];
	int ret = BrotliEncoderCompress(clevel, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, uncompressedSize, (const uint8_t*)inputData, &compressedBufSize, (uint8_t*)(*outputData));
	// CLI default lgwin is 24, encode.h default is 22; why do these compression formats always make their CLI different?
	return compressedBufSize; // Brotli takes the buffer size as an input and changes it to the compressed size
}

uint64_t CompressionSchemeFunctions_Brotli::Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept {
	//TODO (Brotli doesn't have a frame format)
	return 0;
}

uint64_t CompressionSchemeFunctions_Brotli::Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept {
	*outputData = new char[originalSize];
	size_t size = originalSize;
	BrotliDecoderResult ret = BrotliDecoderDecompress(compressedSize, (const uint8_t*)inputData, &size, (uint8_t*)(*outputData));
	return size;
}

uint64_t CompressionSchemeFunctions_Brotli::Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept {
	//TODO
	return 0;
}

CompressionSchemeFunctions_Brotli::~CompressionSchemeFunctions_Brotli() {
	//TODO?
}
#endif
