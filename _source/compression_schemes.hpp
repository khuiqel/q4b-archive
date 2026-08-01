#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "q4b.hpp"

struct CompressionSchemeFunctions {
	[[nodiscard]] virtual uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept = 0;
	virtual uint64_t GetMaxSize() const = 0;
	virtual ~CompressionSchemeFunctions() = default;
};

#ifdef Q4B_ENABLE_LZ4
struct CompressionSchemeFunctions_Lz4 final : public CompressionSchemeFunctions {
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	uint64_t GetMaxSize() const override;
	~CompressionSchemeFunctions_Lz4();
};
#endif

#ifdef Q4B_ENABLE_ZSTD
struct CompressionSchemeFunctions_Zstd final : public CompressionSchemeFunctions {
	//TODO: store cctx
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	uint64_t GetMaxSize() const override;
	~CompressionSchemeFunctions_Zstd();
};
#endif

#ifdef Q4B_ENABLE_BROTLI
struct CompressionSchemeFunctions_Brotli final : public CompressionSchemeFunctions {
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	uint64_t GetMaxSize() const override;
	~CompressionSchemeFunctions_Brotli();
};
#endif
