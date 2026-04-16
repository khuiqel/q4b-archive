#include "gui_data.hpp"
#include <algorithm>
#include <string>
#include <climits> //INT_MAX

#include <zstd.h>
#include <lz4frame.h>
#include <iostream>

std::vector<char*> GuiData::zstd_level_arr;
std::vector<int> GuiData::zstd_level_num;
std::vector<char*> GuiData::lz4_level_arr;
std::vector<int> GuiData::lz4_level_num;

void GuiData::InitializeArrays() {
	if (!zstd_level_arr.empty() || !lz4_level_arr.empty()) {
		return;
	}

	// Zstd

	for (int i = 1; i <= ZSTD_maxCLevel(); i++) {
		std::string level = std::to_string(i);
		if (i >= 20) {
			level += " --ultra";
		} else if (i == ZSTD_defaultCLevel()) {
			level += " (default)";
		}
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		zstd_level_arr.push_back(level_str);
		zstd_level_num.push_back(i);
	}

	const char* level = "--max (NOT RECOMMENDED)";
	char* level_str = new char[sizeof("--max (NOT RECOMMENDED)")+1];
	std::copy(level, level+sizeof("--max (NOT RECOMMENDED)"), level_str);
	zstd_level_arr.push_back(level_str);
	zstd_level_num.push_back(INT_MAX);

	zstd_level_arr.shrink_to_fit();
	zstd_level_num.shrink_to_fit();

	// LZ4

	for (int i = 1; i <= LZ4F_compressionLevel_max(); i++) {
		std::string level = std::to_string(i);
		char* level_str = new char[level.size()+1];
		std::copy(level.begin(), level.end(), level_str);
		level_str[level.size()] = '\0';
		lz4_level_arr.push_back(level_str);
		lz4_level_num.push_back(i);
	}

	lz4_level_arr.shrink_to_fit();
	lz4_level_num.shrink_to_fit();
}

GuiData::GuiData() {
	InitializeArrays();
	zstd_level_idx = std::distance(zstd_level_arr.begin(), std::find(zstd_level_arr.begin(), zstd_level_arr.end(), std::to_string(ZSTD_defaultCLevel()) + " (default)"));
	lz4_level_idx  = std::distance(lz4_level_arr.begin(),  std::find(lz4_level_arr.begin(),  lz4_level_arr.end(),  std::to_string(1)));
}
