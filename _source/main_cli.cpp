#include <iostream>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include "q4b.hpp"

static void WriteFile(const std::filesystem::path& output, const char* data, size_t size) {
	std::ofstream o(output, std::ios::binary);
	o.write(data, size);
}

int main(int argc, char** argv) {

	CLI::App app("");
	// app.set_help_flag();
	// app.set_help_all_flag("-h,--help", "Print this help message and exit");
	CLI::App* subcom_archive    = app.add_subcommand("archive",    "Create Q4B Archives");
	CLI::App* subcom_unpack     = app.add_subcommand("unpack",     "Unpack Q4B Archives");
	CLI::App* subcom_inspect    = app.add_subcommand("inspect",    "Inspect Q4B Archives");
	CLI::App* subcom_compress   = app.add_subcommand("compress",   "Single file compression");
	CLI::App* subcom_decompress = app.add_subcommand("decompress", "Single file decompression");
	app.require_subcommand(1, 1);

	subcom_inspect->add_option("input_file", "Input file")->required();

	subcom_compress->add_option("input_file", "Input file")->required();
	subcom_compress->add_option("scheme", "Compression scheme")->required();
	subcom_compress->add_option("level", "Compression level (scheme-specific)")->required(); //TODO: not required?
	subcom_compress->add_option("-o", "Output folder");
	subcom_compress->add_flag("-m", "Do *not* write metadata (frame header)"); //TODO
	subcom_compress->add_flag("-y", "Overwrite without asking");

	subcom_decompress->add_option("input_file", "Input file")->required();
	subcom_decompress->add_option("scheme", "Compression scheme (optional override)");
	subcom_decompress->add_option("-o", "Output folder");
	subcom_decompress->add_flag("-y", "Overwrite without asking");
	
	CLI11_PARSE(app, argc, argv);

	auto* subcom = app.get_subcommands()[0];
	if (subcom == subcom_archive) {
		//TODO
	} else if (subcom == subcom_unpack) {
		//TODO
	} else if (subcom == subcom_inspect) {

		const std::string input = subcom_inspect->get_option_no_throw("input_file")->as<std::string>();
		q4b::ArchiveHeader archiveHeader;
		std::vector<q4b::ArchivedFileHeader> fileList;
		q4b::ReadArchiveHeader(input, archiveHeader, fileList);
		std::cout << archiveHeader.magic << " "
		          << archiveHeader.flags << " "
		          << archiveHeader.version << " "
		          << archiveHeader.num_files << " "
		          << archiveHeader.self_hash << std::endl;
		for (const q4b::ArchivedFileHeader& file_header : fileList) {
			std::cout << file_header.path << " "
			          << q4b::CompressionToStr(file_header.compression_type) << " "
			          << file_header.compressed_size << " "
			          << file_header.uncompressed_size << " "
			          << file_header.compressed_hash << " "
			          << file_header.uncompressed_hash << std::endl;
		}

	} else if (subcom == subcom_compress) {

		const std::string input = subcom_compress->get_option_no_throw("input_file")->as<std::string>();
		const std::string scheme = subcom_compress->get_option_no_throw("scheme")->as<std::string>();
		const std::string level = subcom_compress->get_option_no_throw("level")->as<std::string>();
		const std::string output = subcom_compress->get_option_no_throw("-o")->empty() ?
		                           "." : subcom_compress->get_option_no_throw("-o")->as<std::string>();

		// if (std::filesystem::exists(input) && std::filesystem::is_directory(output)) {
		char* file_data;
		const int64_t file_size = q4b::LoadFileIntoMemory(input, &file_data);
		if (file_size == -1) {
			//TODO
		}

		//TODO
		char* compressed_file_data;
		size_t compressed_size = q4b::CompressLz4Data(file_data, std::filesystem::file_size(input), &compressed_file_data, std::stoi(level));

		WriteFile((output / std::filesystem::path(input).filename()).string() + ".lz4", compressed_file_data, compressed_size);
		delete[] file_data;

	} else if (subcom == subcom_decompress) {
		//TODO
	} else {
		//oh no
	}

	return 0;
}
