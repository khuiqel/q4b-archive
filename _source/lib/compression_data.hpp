#pragma once
#include <cstdint>
#include "../q4b.hpp"

struct CompressionSchemeFunctions {
	// TODO: should core info get duplicated?

	/* Compress() and Decompress() are for Q4B archives. As the archive stores metadata like
	 * length and hashes, Compress() and Decompress() don't create/use such information, thus
	 * typically creating blocks instead of frames.
	 *
	 * Compress_GenericExport() and Decompress_UnknownSize() are for general use with other files.
	 * They will store metadata if the frame format has such information. If the "don't create
	 * metadata" flag is specified, hashes will not be created but size will still be stored. If
	 * the compression scheme does not have a frame format, then the functions will do their best
	 * to fulfill the request, which may be identical to Compress() and Decompress().
	 */

	virtual uint64_t GetMaxSize() const = 0;
	[[nodiscard]] virtual uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept = 0;
	[[nodiscard]] virtual uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept = 0;
	virtual ~CompressionSchemeFunctions() = default;
};

#ifdef Q4B_ENABLE_LZ4
struct CompressionSchemeFunctions_Lz4 final : public CompressionSchemeFunctions {
	uint64_t GetMaxSize() const override;
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	~CompressionSchemeFunctions_Lz4();
};
#endif

#ifdef Q4B_ENABLE_ZSTD
struct CompressionSchemeFunctions_Zstd final : public CompressionSchemeFunctions {
	//TODO: store cctx
	uint64_t GetMaxSize() const override;
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	~CompressionSchemeFunctions_Zstd();
};
#endif

#ifdef Q4B_ENABLE_BROTLI
struct CompressionSchemeFunctions_Brotli final : public CompressionSchemeFunctions {
	uint64_t GetMaxSize() const override;
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	~CompressionSchemeFunctions_Brotli();
};
#endif

#ifdef Q4B_ENABLE_STB
struct CompressionSchemeFunctions_Stb final : public CompressionSchemeFunctions {
	uint64_t GetMaxSize() const override;
	uint64_t Compress(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Compress_GenericExport(int clevel, q4b::Q4B_CompressionFileFlags flags, const void* inputData, uint64_t uncompressedSize, void** outputData) const noexcept override;
	uint64_t Decompress(const void* inputData, uint64_t compressedSize, void** outputData, uint64_t originalSize) const noexcept override;
	uint64_t Decompress_UnknownSize(const void* inputData, uint64_t compressedSize, void** outputData) const noexcept override;
	~CompressionSchemeFunctions_Stb();
};
#endif
