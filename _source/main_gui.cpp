// Dear ImGui: standalone example application for SDL3 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL3.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <stdio.h>
#include <SDL3/SDL.h>

#include "q4b.hpp"
#include "gui_data.hpp"
#include <iostream>

GuiData gdata;
std::vector<q4b::CompressionFile> FILE_LIST;

// Main code
int main(int argc, char** argv)
{
    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
	#if SDL_JOYSTICK
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	#else
	if (!SDL_Init(SDL_INIT_VIDEO))
	#endif
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    // Create window with SDL_Renderer graphics context
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_Renderer example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);
    style.FontSizeBase = 24.0f;
    io.Fonts->AddFontFromFileTTF("res/NotoSans-Regular.ttf");

    // Our state
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	{
		SDL_Surface* icon = SDL_LoadPNG("res/q4b-favicon.png");
		SDL_SetWindowIcon(window, icon);
		SDL_DestroySurface(icon);
	}
	FILE_LIST.push_back({ argv[0], q4b::CompressionScheme::zstd, 3, 1 });

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
			switch(event.type) {
				case SDL_EVENT_QUIT:
					done = true;
					break;
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					if (event.window.windowID == SDL_GetWindowID(window)) {
						done = true;
					}
					break;

				case SDL_EVENT_DROP_FILE:
					if (std::filesystem::exists(event.drop.data)) [[likely]] {
						//TODO
						std::string path = std::string(event.drop.data);
						std::replace(path.begin(), path.end(), '\\', '/');
						FILE_LIST.push_back({ path, (q4b::CompressionScheme)gdata.compression_type_idx, gdata.zstd_level_num[gdata.zstd_level_idx], 1 });
					} else {
						printf("Not a file: %s\n", event.drop.data);
					}
					break;
			}
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

		ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
		if (ImGui::BeginTabBar("MainTabBar", 0)) {
			if (ImGui::BeginTabItem("Q4B Archiving")) {
				ImGui::Text("Drop files here");

				const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
				static ImGui_ExampleSelectionWithDeletion selection;
				selection.UserData = (void*)&FILE_LIST;
				selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int idx) { return (ImGuiID)idx; };

				const int ITEMS_COUNT = FILE_LIST.size();
				ImGui::Text("Selection: %d/%d", selection.Size, ITEMS_COUNT);
				if (ImGui::BeginTable("Selection", 4, table_flags, ImVec2(0.0f, ImGui::GetFontSize() * 20)))
				{
					ImGui::TableSetupColumn("File");
					ImGui::TableSetupColumn("Compression Format");
					ImGui::TableSetupColumn("Compression Level");
					ImGui::TableSetupColumn("Hash?");
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					const ImGuiMultiSelectFlags selection_flags = ImGuiMultiSelectFlags_BoxSelect1d | ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
					ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(selection_flags, selection.Size, ITEMS_COUNT);
					selection.ApplyRequests(ms_io);

					const bool want_delete = ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) && (selection.Size > 0);
					const int item_curr_idx_to_focus = want_delete ? selection.ApplyDeletionPreLoop(ms_io, ITEMS_COUNT) : -1;

					ImGuiListClipper clipper;
					clipper.Begin(ITEMS_COUNT);
					if (ms_io->RangeSrcItem != -1)
						clipper.IncludeItemByIndex((int)ms_io->RangeSrcItem); // Ensure RangeSrc item is not clipped.
					while (clipper.Step())
					{
						for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++)
						{
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::PushID(n);
							bool item_is_selected = selection.Contains((ImGuiID)n);
							ImGui::SetNextItemSelectionUserData(n);
							ImGui::Selectable(FILE_LIST[n].getFilepath(), &item_is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
							if (item_curr_idx_to_focus == n)
								ImGui::SetKeyboardFocusHere(-1);

							ImGui::TableNextColumn();
							ImGui::TextUnformatted(q4b::CompressionToStr(FILE_LIST[n].compression_type));

							ImGui::TableNextColumn();
							ImGui::TextUnformatted(std::to_string(FILE_LIST[n].compression_level).c_str());

							ImGui::TableNextColumn();
							ImGui::CheckboxFlags(("##" + std::to_string(n)).c_str(), &FILE_LIST[n].compression_flags, 1);
							ImGui::PopID();
						}
					}

					ms_io = ImGui::EndMultiSelect();
					selection.ApplyRequests(ms_io);
					if (want_delete)
						selection.ApplyDeletionPostLoop(ms_io, FILE_LIST, item_curr_idx_to_focus);
					ImGui::EndTable();
				}
				//TODO: option to set what the root path is (for generating the relative filepaths)

				if (ImGui::Button("Prune Existence")) {
					q4b::ExistencePrune(FILE_LIST);
				}

				static const char* compression_types[3] = { q4b::CompressionToStr((q4b::CompressionScheme)0), q4b::CompressionToStr((q4b::CompressionScheme)1), q4b::CompressionToStr((q4b::CompressionScheme)2) };
				ImGui::Combo("Compression Format", &gdata.compression_type_idx, compression_types, IM_COUNTOF(compression_types));
				ImGui::Combo("Zstd Compression Level", &gdata.zstd_level_idx, GuiData::zstd_level_arr.data(), GuiData::zstd_level_arr.size());
				ImGui::Combo("LZ4 Compression Level", &gdata.lz4_level_idx, GuiData::lz4_level_arr.data(), GuiData::lz4_level_arr.size());

				if (ImGui::Button("Create archive.q4b")) {
					q4b::WriteArchive(FILE_LIST, "archive.q4b");
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Q4B Unpacking")) {
				ImGui::TextUnformatted("TODO");

				if (ImGui::Button("Preview archive.q4b")) {
					//TODO: select which ones to unpack
				}
				if (ImGui::Button("Decode archive.q4b")) {
					q4b::DecodeArchive("archive.q4b", "output");
				}
				if (ImGui::Button("Read header of archive.q4b")) {
					gdata.viewingArchiveFileList.clear();
					q4b::ReadArchiveHeader("archive.q4b", gdata.viewingArchiveHeader, gdata.viewingArchiveFileList);
					std::cout << gdata.viewingArchiveHeader.magic << " "
					          << gdata.viewingArchiveHeader.flags << " "
					          << gdata.viewingArchiveHeader.version << " "
					          << gdata.viewingArchiveHeader.num_files << " "
					          << gdata.viewingArchiveHeader.self_hash << std::endl;
					for (const q4b::ArchivedFileHeader& file_header : gdata.viewingArchiveFileList) {
						std::cout << file_header.path << " "
						          << q4b::CompressionToStr(file_header.compression_type) << " "
						          << file_header.compressed_size << " "
						          << file_header.uncompressed_size << " "
						          << file_header.compressed_hash << " "
						          << file_header.uncompressed_hash << std::endl;
					}
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Quick Compression")) {
				ImGui::Text("Drop files here");
				//shares the same list as the archiving window

				if (ImGui::Button("Create q4b-gui.zstd (TODO)")) {
					//q4b::compressZstdFile(argv[0], std::string(argv[0]) + ".zstd");
				}
				if (ImGui::Button("Decompress q4b-gui.zstd (TODO)")) {
					//q4b::decompressZstdFile(std::string(argv[0]) + ".zstd", std::string(argv[0]) + "-decompressed");
				}

				//TODO: this needs to do an lz4 frame, not block
				if (ImGui::Button("Create q4b-gui.lz4 (TODO)")) {
					//q4b::compressLz4File(argv[0], std::string(argv[0]) + ".lz4");
				}
				if (ImGui::Button("Decompress q4b-gui.lz4 (TODO)")) {
					//q4b::decompressLz4File(std::string(argv[0]) + ".lz4", std::string(argv[0]) + "-decompressed", std::filesystem::file_size(argv[0]));
				}

				if (ImGui::Button("Prune Existence")) {
					q4b::ExistencePrune(FILE_LIST);
				}

				ImGui::EndTabItem();
			}

			/*
			if (ImGui::BeginTabItem("Help")) {
				//TODO: information for each compression format (and q4b)
				ImGui::EndTabItem();
			}
			*/

			if (ImGui::BeginTabItem("About")) {
				ImGui::Text("License: GNU General Public License v3.0");
				ImGui::Text("SPDX-License-Identifier: GPL-3.0-only");
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				ImGui::TextLinkOpenURL("GitHub link", "https://github.com/khuiqel/q4b-archive");

				// #ifndef NDEBUG
				ImGui::Checkbox("Demo Window", &show_demo_window);
				// #endif

				// ImGui::NewLine();
				//TODO: enabled modules (zstd, lz4, others)

				ImGui::NewLine();
				if (ImGui::TreeNodeEx("Extra Buttons", ImGuiTreeNodeFlags_FramePadding)) {
					if (ImGui::Button("Toggle window decorations (title bar & outline)")) {
						SDL_SetWindowBordered(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_BORDERLESS));
					}
					if (ImGui::Button("Maximize window")) {
						SDL_MaximizeWindow(window);
					}
					if (ImGui::Button("Un-maximize window")) {
						SDL_RestoreWindow(window);
					}
					if (ImGui::Button("Minimize window")) {
						SDL_MinimizeWindow(window);
					}
					ImGui::TreePop();
				}
				if (ImGui::Button("Close Application")) {
					done = true;
				}
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::End();

        // Rendering
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
