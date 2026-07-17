#include <iostream>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include "q4b.hpp"

static void WriteFile(const std::filesystem::path& output, const char* data, size_t size) {
	std::ofstream o(output, std::ios::binary);
	o.write(data, size);
}

static std::string SchemeToExtension(q4b::CompressionScheme scheme, bool metadata) {
	switch (scheme) {
		default: [[fallthrough]];
		case q4b::CompressionScheme::Uncompressed: return ".uncompressed"; //TODO
		case q4b::CompressionScheme::lz4:          return (metadata ? ".lz4f" : ".lz4"); //TODO
		case q4b::CompressionScheme::zstd:         return ".zst";
		case q4b::CompressionScheme::brotli:       return ".br";
	}
}

static void ReadArchiveInputFile(const std::filesystem::path& input, std::vector<q4b::CompressionFile>& file_list) {
	std::ifstream f(input);
	std::string line;
	while (std::getline(f, line)) {
		size_t pos_start, pos_end;
		pos_start = line.find_first_of(" ");
		std::string filename = line.substr(0, pos_start);
		pos_end = line.find_first_of(" ", pos_start+1);
		std::string scheme = line.substr(pos_start+1, pos_end - pos_start - 1);
		pos_start = pos_end;
		pos_end = line.find_first_of(" ", pos_start+1);
		std::string level = line.substr(pos_start+1, pos_end - pos_start - 1);

		file_list.push_back({ filename, q4b::CompressionScheme::lz4, std::stoi(level) });
	}
}

int main(int argc, char** argv) {

	CLI::App app("");
	// app.set_help_flag();
	// app.set_help_all_flag("-h,--help", "Print this help message and exit");
	CLI::App* subcom_archive    = app.add_subcommand("archive",    "Create Q4B Archives");
	CLI::App* subcom_unpack     = app.add_subcommand("unpack",     "Unpack Q4B Archives");
	// CLI::App* subcom_patch      = app.add_subcommand("patch",      "Patch (partial overwrite) Q4B Archives");
	CLI::App* subcom_inspect    = app.add_subcommand("inspect",    "Inspect Q4B Archives");
	CLI::App* subcom_compress   = app.add_subcommand("compress",   "Single file compression");
	CLI::App* subcom_decompress = app.add_subcommand("decompress", "Single file decompression");
	app.require_subcommand(1, 1);

	subcom_archive->add_option("input_file", "Input file (format: file scheme level, on each line)")->required();
	subcom_archive->add_option("output_archive", "Output Q4B archive")->required();

	subcom_unpack->add_option("input_archive", "Input Q4B archive")->required();
	subcom_unpack->add_option("input_file", "Input file (format: file, on each line)")->required(); //TODO: list of files OR single file (-f?)
	subcom_unpack->add_option("-o", "Output folder");

	subcom_inspect->add_option("input_archive", "Input Q4B archive")->required();
	subcom_inspect->add_flag("-c", "Calculate and verify metadata"); //TODO

	subcom_compress->add_option("input_file", "Input file")->required();
	subcom_compress->add_option("scheme", "Compression scheme")->required();
	subcom_compress->add_option("level", "Compression level (scheme-specific)")->required(); //TODO: not required?
	subcom_compress->add_option("-o", "Output folder");
	subcom_compress->add_flag("-m", "Do NOT write metadata (frame header)");
	subcom_compress->add_flag("-y", "Overwrite without asking"); //TODO

	subcom_decompress->add_option("input_file", "Input file")->required();
	subcom_decompress->add_option("scheme", "Compression scheme (optional override)");
	subcom_decompress->add_option("-o", "Output folder");
	subcom_decompress->add_flag("-y", "Overwrite without asking"); //TODO
	
	CLI11_PARSE(app, argc, argv);

	auto* subcom = app.get_subcommands()[0];
	if (subcom == subcom_archive) {

		const std::string FILES      = subcom_archive->get_option_no_throw("input_file")->as<std::string>();
		const std::string ARCHIVE    = subcom_archive->get_option_no_throw("output_archive")->as<std::string>();

		std::vector<q4b::CompressionFile> file_list;
		ReadArchiveInputFile(FILES, file_list);

		std::vector<q4b::ErrorMessage> messages;
		q4b::WriteArchive(file_list, ".", ARCHIVE, 4, &messages);

	} else if (subcom == subcom_unpack) {

		const std::string ARCHIVE    = subcom_archive->get_option_no_throw("input_archive")->as<std::string>();
		const std::string FILES      = subcom_archive->get_option_no_throw("input_file")->as<std::string>();
		const std::string OUTPUT_DIR = subcom_archive->get_option_no_throw("-o")->empty() ?
		                               "." : subcom_archive->get_option_no_throw("-o")->as<std::string>();

		std::vector<q4b::CompressionFile> file_list;
		ReadArchiveInputFile(FILES, file_list);

		q4b::DecodeArchive(ARCHIVE, OUTPUT_DIR);

	} else if (subcom == subcom_inspect) {

		const std::string ARCHIVE = subcom_inspect->get_option_no_throw("input_archive")->as<std::string>();

		q4b::ArchiveHeader archiveHeader;
		std::vector<q4b::ArchivedFileHeader> fileList;
		q4b::ReadArchiveHeader(ARCHIVE, archiveHeader, fileList);

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

		const std::string INPUT      = subcom_compress->get_option_no_throw("input_file")->as<std::string>();
		const std::string SCHEME     = subcom_compress->get_option_no_throw("scheme")->as<std::string>();
		const std::string LEVEL      = subcom_compress->get_option_no_throw("level")->as<std::string>();
		const std::string OUTPUT_DIR = subcom_compress->get_option_no_throw("-o")->empty() ?
		                               "." : subcom_compress->get_option_no_throw("-o")->as<std::string>();
		const bool WRITE_METADATA = subcom_compress->get_option_no_throw("-m")->empty();

		// if (std::filesystem::exists(INPUT) && std::filesystem::is_directory(OUTPUT_DIR)) {
		char* file_data;
		const int64_t file_size = q4b::LoadFileIntoMemory(INPUT, &file_data);
		if (file_size == -1) {
			std::cout << "ERROR: file not found\n";
			return 1;
		}

		//TODO
		char* compressed_file_data;
		size_t compressed_size = q4b::CompressLz4Data(file_data, std::filesystem::file_size(INPUT), &compressed_file_data, std::stoi(LEVEL));

		WriteFile((OUTPUT_DIR / std::filesystem::path(INPUT).filename()) / SchemeToExtension(q4b::CompressionScheme::lz4, WRITE_METADATA), compressed_file_data, compressed_size);
		delete[] file_data;

	} else if (subcom == subcom_decompress) {

		/*
		const std::string INPUT      = subcom_decompress->get_option_no_throw("input_file")->as<std::string>();
		const std::string SCHEME     = subcom_decompress->get_option_no_throw("scheme")->as<std::string>();
		const std::string OUTPUT_DIR = subcom_decompress->get_option_no_throw("-o")->empty() ?
		                               "." : subcom_decompress->get_option_no_throw("-o")->as<std::string>();

		char* file_data;
		const int64_t file_size = q4b::LoadFileIntoMemory(INPUT, &file_data);
		if (file_size == -1) {
			std::cout << "ERROR: file not found\n";
			return 1;
		}

		char* decompressed_file;
		size_t decompressed_size;
		if (std::filesystem::path(INPUT).extension() == ".lz4f") {
			decompressed_size = q4b::DecompressLz4Data_Metadata(file_data, file_size, &decompressed_file, uncompressed_size);
		} else if (std::filesystem::path(INPUT).extension() == ".lz4") {
			decompressed_size = q4b::DecompressLz4Data(file_data, file_size, &decompressed_file, uncompressed_size);
		} else {
			//TODO
		}

		WriteFile((OUTPUT_DIR / std::filesystem::path(INPUT).stem()).string(), decompressed_file, uncompressed_size);
		*/

	} else {
		//oh no
	}

	return 0;
}
