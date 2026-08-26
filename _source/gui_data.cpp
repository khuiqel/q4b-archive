#include "gui_data.hpp"
#include <algorithm>
#include <string>
#include <thread> //hardware_concurrency

int GuiData::threadCountMax = 1;
std::vector<CompressionSchemeInfo*> GuiData::compressionSchemes;

void GuiData::Initialize() {
	if (!compressionSchemes.empty()) {
		return;
	}

	compressionSchemes = {
		new CompressionSchemeInfo_Uncompressed(),
		#ifdef Q4B_ENABLE_LZ4
		new CompressionSchemeInfo_Lz4(),
		#endif
		#ifdef Q4B_ENABLE_ZSTD
		new CompressionSchemeInfo_Zstd(),
		#endif
		#ifdef Q4B_ENABLE_BROTLI
		new CompressionSchemeInfo_Brotli(),
		#endif
		#ifdef Q4B_ENABLE_STB
		new CompressionSchemeInfo_Stb(),
		#endif
	};
	for (auto c : compressionSchemes) {
		c->Initialize_Gui();
	}

	threadCountMax = std::max(1u, std::thread::hardware_concurrency());
}

void GuiData::Uninitialize() {
	for (auto c : compressionSchemes) {
		delete c;
	}
}

GuiData::GuiData() {
	Initialize();
	threadCount = 4;

	#if defined(Q4B_ENABLE_ZSTD) && defined(Q4B_ENABLE_LZ4)
	// Zstd and LZ4 exist, use Zstd
	compressionScheme_idx = 2;
	#elif defined(Q4B_ENABLE_ZSTD) || defined(Q4B_ENABLE_LZ4)
	// Use Zstd or LZ4, whichever exists
	compressionScheme_idx = 1;
	#else
	// Uncompressed
	compressionScheme_idx = 0;
	#endif

	compressionLevel_idx = compressionSchemes[compressionScheme_idx]->clevel_default_idx;
}
