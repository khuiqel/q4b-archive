#include "gui_data.hpp"
#include <thread> //hardware_concurrency

int GuiData::threadCountMax = 1;

void GuiData::Initialize() {
	if (q4b::SCHEME_INFO[0]->clevel_str.size() > 0) {
		return;
	}

	for (auto c : q4b::SCHEME_INFO) {
		c->Initialize_Gui();
	}

	threadCountMax = std::max(1u, std::thread::hardware_concurrency());
}

GuiData::GuiData() {
	Initialize();
	threadCount = 4;

	#if defined(Q4B_ENABLE_LZ4) && defined(Q4B_ENABLE_ZSTD)
	// Zstd and LZ4 exist, use Zstd
	compressionScheme_idx = 2;
	#elif defined(Q4B_ENABLE_LZ4) || defined(Q4B_ENABLE_ZSTD)
	// Use Zstd or LZ4, whichever exists
	compressionScheme_idx = 1;
	#else
	// Uncompressed
	compressionScheme_idx = 0;
	#endif

	compressionLevel_idx = q4b::SCHEME_INFO[compressionScheme_idx]->clevel_default_idx;
}
