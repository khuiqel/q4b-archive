#pragma once
#include <imgui.h>
#include <SDL3/SDL.h>

namespace ImGuiHelpers {

inline void Tooltip(const char* text, float relative_width = 35.0f) {
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * relative_width);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

inline void HelpMarker(const char* desc) {
	ImGui::TextDisabled("(?)");
	Tooltip(desc);
}

inline bool LoadPNGFromFile(const char* file_name, SDL_Renderer* renderer, SDL_Texture** out_texture) {
	SDL_Surface* surface = SDL_LoadPNG(file_name);
	if (surface == nullptr)
		return false;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	if (texture == nullptr) {
		SDL_DestroySurface(surface);
		return false;
	}

	*out_texture = texture;

	SDL_DestroySurface(surface);
	return true;
}

} // namespace ImGuiHelpers
