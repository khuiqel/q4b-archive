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

auto filepathCleaningFunc = [] (ImGuiInputTextCallbackData* data) {
	if (data->EventChar == '\\') {
		data->EventChar = '/';
	}
	else if (data->EventChar == '*'  ||
	         data->EventChar == '\"' ||
	         data->EventChar == '?'  ||
	         data->EventChar == '<'  ||
	         data->EventChar == '>'  ||
	         data->EventChar == '|')
		{ return 1; }
	return 0;
};

char root_file_path[1024];
char archive_file_path[1024] = "archive.q4b";
char output_file_path[1024] = "output";

#include <thread>
#include <atomic>
std::atomic_bool thread_func_working = false;
std::atomic_bool thread_func_exit_early = false;
std::atomic_int thread_files_completed = 0;

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
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_Renderer example", (int)(1600 * main_scale), (int)(1000 * main_scale), window_flags);
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

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	bool show_demo_window = true;
	// bool set_startup_tab = false;
	bool rootDirIsLocked = true;
	bool ret;

	// Window icon
	{
		SDL_Surface* icon = SDL_LoadPNG("res/q4b-favicon.png");
		ret = SDL_SetWindowIcon(window, icon);
		SDL_DestroySurface(icon);
	}

	// Root folder for relative paths
	{
		std::string root_folder = std::filesystem::current_path().generic_string(); // .generic_string() converts slashes on Windows
		strncpy(root_file_path, root_folder.c_str(), root_folder.size());
	}
	FILE_LIST.push_back({ argv[0], q4b::CompressionScheme::zstd, 3 });

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
					if (rootDirIsLocked) {
						std::filesystem::path path(event.drop.data);
						int level;
						switch ((q4b::CompressionScheme)gdata.compression_type_idx) {
							default: [[fallthrough]];
							case q4b::CompressionScheme::Uncompressed: level = 0;
							case q4b::CompressionScheme::zstd: level = GuiData::zstd_level_num[gdata.zstd_level_idx];
							case q4b::CompressionScheme::lz4:  level = GuiData::lz4_level_num[gdata.lz4_level_idx];
						}
						FILE_LIST.push_back({ path.lexically_relative(root_file_path).generic_string(), (q4b::CompressionScheme)gdata.compression_type_idx, level });
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

		const bool THREAD_IS_WORKING = thread_func_working.load();
		ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
		if (ImGui::BeginTabBar("MainTabBar", 0)) {
			if (ImGui::BeginTabItem("Q4B Archiving")) {
				if (THREAD_IS_WORKING) { ImGui::BeginDisabled(); }
				if (!rootDirIsLocked) { ImGui::BeginDisabled(); }
				ImGui::Text("Drop files here");

				const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
				static ImGui_ExampleSelectionWithDeletion selection;
				selection.UserData = (void*)&FILE_LIST;
				selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int idx) { return (ImGuiID)idx; };

				const int ITEMS_COUNT = FILE_LIST.size();
				ImGui::Text("Selection: %d/%d", selection.Size, ITEMS_COUNT);
				if (ImGui::BeginTable("Selection", 3, table_flags, ImVec2(ImGui::GetContentRegionAvail().x * .75f, ImGui::GetFontSize() * 20)))
				{
					ImGui::TableSetupColumn("File");
					ImGui::TableSetupColumn("Scheme");
					ImGui::TableSetupColumn("Level");
					// ImGui::TableSetupColumn("Write Metadata?");
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					const ImGuiMultiSelectFlags selection_flags = ImGuiMultiSelectFlags_BoxSelect1d | ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
					ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(selection_flags, selection.Size, ITEMS_COUNT);
					selection.ApplyRequests(ms_io);

					const bool want_delete = ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) && (selection.Size > 0);
					const int item_curr_idx_to_focus = want_delete ? selection.ApplyDeletionPreLoop(ms_io, ITEMS_COUNT) : -1;

					for (int n = 0; n < ITEMS_COUNT; n++)
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
						ImGui::TextUnformatted(q4b::CompressionToStr(FILE_LIST[n].data.compression_type));

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(std::to_string(FILE_LIST[n].compression_level).c_str());

						// ImGui::TableNextColumn();
						// ImGui::CheckboxFlags(("##" + std::to_string(n)).c_str(), &FILE_LIST[n].compression_flags, 1);
						ImGui::PopID();
					}

					ms_io = ImGui::EndMultiSelect();
					selection.ApplyRequests(ms_io);
					if (want_delete)
						selection.ApplyDeletionPostLoop(ms_io, FILE_LIST, item_curr_idx_to_focus);
					ImGui::EndTable();
				}
				if (!rootDirIsLocked) { ImGui::EndDisabled(); }

				ImGui::SeparatorText("Change");
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.3f);
				ImGui::Combo("Compression Scheme", &gdata.compression_type_idx, GuiData::compression_types.data(), GuiData::compression_types.size());
				ImGui::Combo("Zstd Compression Level", &gdata.zstd_level_idx, GuiData::zstd_level_arr.data(), GuiData::zstd_level_arr.size());
				ImGui::Combo("LZ4 Compression Level", &gdata.lz4_level_idx, GuiData::lz4_level_arr.data(), GuiData::lz4_level_arr.size());
				ImGui::PopItemWidth();

				if (!rootDirIsLocked) { ImGui::BeginDisabled(); }
				if (ImGui::Button("Change Files")) {
					for (int i = 0; i < ITEMS_COUNT; i++) {
						if (selection.Contains((ImGuiID)i)) {
							FILE_LIST[i].data.compression_type = (q4b::CompressionScheme)gdata.compression_type_idx;
							switch (FILE_LIST[i].data.compression_type) {
								default: [[fallthrough]];
								case q4b::CompressionScheme::Uncompressed:
									FILE_LIST[i].compression_level = 0;
									break;

								case q4b::CompressionScheme::zstd:
									FILE_LIST[i].compression_level = GuiData::zstd_level_num[gdata.zstd_level_idx];
									break;
								case q4b::CompressionScheme::lz4:
									FILE_LIST[i].compression_level = GuiData::lz4_level_num[gdata.lz4_level_idx];
									break;
							}
						}
					}
				}
				if (!rootDirIsLocked) { ImGui::EndDisabled(); }

				ImGui::SeparatorText("Configuration");
				if (ImGui::Button(rootDirIsLocked ? "Unlock" : "Lock", { ImGui::CalcTextSize("Unlock").x + 2*style.FramePadding.x, 0.0f })) {
					if (rootDirIsLocked) {
						FILE_LIST.clear();
					}
					rootDirIsLocked = !rootDirIsLocked;
				}
				ImGui::SameLine();
				if (rootDirIsLocked) { ImGui::BeginDisabled(); }
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
				ImGui::InputText("Root folder", root_file_path, IM_COUNTOF(root_file_path), ImGuiInputTextFlags_CallbackCharFilter, filepathCleaningFunc);
				ImGui::PopItemWidth();
				if (rootDirIsLocked) { ImGui::EndDisabled(); }

				static constexpr uint32_t threadMin = 1;
				ImGui::SliderScalar("Thread count", ImGuiDataType_S32, &gdata.threadCount, &threadMin, &GuiData::threadCountMax);
				//TODO: another thread count for how many workers to create; the one above is how many helper threads each worker thread gets

				if (!rootDirIsLocked) { ImGui::BeginDisabled(); }
				if (ImGui::Button("Prune Existence")) {
					q4b::ExistencePrune(FILE_LIST);
				}
				if (ImGui::BeginItemTooltip()) {
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					ImGui::TextUnformatted("Remove files that no longer exist");
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
				if (!rootDirIsLocked) { ImGui::EndDisabled(); }
				if (THREAD_IS_WORKING) { ImGui::EndDisabled(); }

				ImGui::SeparatorText("Create");
				if (THREAD_IS_WORKING) { ImGui::BeginDisabled(); }
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
				ImGui::InputText("Archive name", archive_file_path, IM_COUNTOF(archive_file_path), ImGuiInputTextFlags_CallbackCharFilter, filepathCleaningFunc);
				ImGui::PopItemWidth();
				if (THREAD_IS_WORKING) { ImGui::EndDisabled(); }

				if (!rootDirIsLocked) { ImGui::BeginDisabled(); }
				if (THREAD_IS_WORKING) {
					const unsigned int files_completed = thread_files_completed.load(std::memory_order_relaxed);
					const float progress = float(files_completed) / float(FILE_LIST.size());
					char buf[32];
					sprintf(buf, "%u/%zu", files_completed, FILE_LIST.size());
					ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);

					const bool EXIT_EARLY = thread_func_exit_early.load(std::memory_order_acquire);
					if (EXIT_EARLY) { ImGui::BeginDisabled(); }
					if (ImGui::Button("Cancel")) {
						thread_func_exit_early.store(true);
					}
					if (EXIT_EARLY) { ImGui::EndDisabled(); }
				} else {
					if (ImGui::Button("Create Archive")) {
						gdata.messages.clear();
						thread_func_working.store(true);
						thread_func_exit_early.store(false);
						thread_files_completed.store(0);
						std::thread t(q4b::WriteArchive_internal<true>, FILE_LIST, root_file_path, archive_file_path, gdata.threadCount, &gdata.messages, &thread_func_working, &thread_func_exit_early, &thread_files_completed);
						t.detach();
						//TODO: should probably make a global thread instead of re-creating one
					}
				}
				if (!rootDirIsLocked) { ImGui::EndDisabled(); }

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Q4B Unpacking")) {
				ImGui::TextUnformatted("TODO");

				if (ImGui::Button("Preview Archive")) {
					//TODO: select which ones to unpack
				}

				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.3f);
				ImGui::InputText("Output folder", output_file_path, IM_COUNTOF(output_file_path), ImGuiInputTextFlags_CallbackCharFilter, filepathCleaningFunc);
				ImGui::PopItemWidth();

				if (ImGui::Button("Decode Archive")) {
					q4b::DecodeArchive(archive_file_path, output_file_path);
				}

				if (ImGui::Button("Read Archive Header")) {
					gdata.viewingArchiveFileList.clear();
					q4b::ReadArchiveHeader(archive_file_path, gdata.viewingArchiveHeader, gdata.viewingArchiveFileList);
					gdata.viewingArchive = true;
				}

				//TODO: select these files for decompression
				if (gdata.viewingArchive) {
					if (ImGui::Button("Stop Viewing")) {
						gdata.viewingArchive = false;
					}
					const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
					if (ImGui::BeginTable("table1", 4, table_flags)) {
						ImGui::TableSetupColumn("File");
						ImGui::TableSetupColumn("Scheme");
						ImGui::TableSetupColumn("Compressed Size");
						ImGui::TableSetupColumn("Uncompressed Size");
						ImGui::TableHeadersRow();

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text(gdata.viewingArchiveHeader.magic);
						ImGui::TableNextColumn();
						ImGui::Text(std::to_string(gdata.viewingArchiveHeader.flags).c_str());
						ImGui::TableNextColumn();
						ImGui::Text(std::to_string(gdata.viewingArchiveHeader.version).c_str());
						ImGui::TableNextColumn();
						ImGui::Text(std::to_string(gdata.viewingArchiveHeader.num_files).c_str());

						for (const q4b::ArchivedFileHeader& file_header : gdata.viewingArchiveFileList) {
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text(file_header.path);
							ImGui::TableNextColumn();
							ImGui::Text(q4b::CompressionToStr(file_header.compression_type));
							ImGui::TableNextColumn();
							ImGui::Text(std::to_string(file_header.compressed_size).c_str());
							ImGui::TableNextColumn();
							ImGui::Text(std::to_string(file_header.uncompressed_size).c_str());
						}
						ImGui::EndTable();
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

	thread_func_exit_early.store(true);

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
