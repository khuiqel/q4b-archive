#include <iostream>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "q4b_helpers.hpp"
#include "app/compression_info.hpp"
#include "lib/compression_data.hpp"

static void WriteFile(const std::filesystem::path& output, const char* data, size_t size) {
	std::ofstream o(output, std::ios::binary);
	o.write(data, size);
}

//TODO: CLI and GUI and q4b_helpers should share this
CompressionSchemeInfo* SCHEME_INFO[] = {
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

// Returns 1 on failure, sets scheme on success
static int ExtToScheme(const std::string& FILE_EXT, q4b::CompressionScheme* scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		for (const auto& ext : info->fileExtensions) {
			if (FILE_EXT == ext) {
				*scheme = info->scheme;
				return 0;
			}
		}
	}
	std::cerr << "ERROR: could not determine scheme\n";
	return 1;
}

// Returns 1 on failure, sets scheme on success
static int StrToScheme(const std::string& SCHEME, q4b::CompressionScheme* scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		for (const auto& name : info->searchNames) {
			if (SCHEME == name) {
				*scheme = info->scheme;
				return 0;
			}
		}
	}
	std::cerr << "ERROR: unknown scheme\n";
	return 1;
}

static std::string SchemeToFileExt(q4b::CompressionScheme scheme) {
	for (const CompressionSchemeInfo* info : SCHEME_INFO) {
		if (scheme == info->scheme) {
			return info->fileExtensions[0];
		}
	}
	return "";
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

	const std::string DESCRIPTION_STR = "Enabled schemes: Uncompressed"
	#ifdef Q4B_ENABLE_LZ4
	", LZ4"
	#endif
	#ifdef Q4B_ENABLE_ZSTD
	", Zstd"
	#endif
	#ifdef Q4B_ENABLE_BROTLI
	", Brotli"
	#endif
	#ifdef Q4B_ENABLE_STB
	", stb"
	#endif
	;

	CLI::App app(DESCRIPTION_STR);
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
	subcom_compress->add_option_group("")->add_flag("--bench", "Benchmark mode");

	subcom_decompress->add_option("input_file", "Input file")->required();
	subcom_decompress->add_option("scheme", "Compression scheme (optional override)");
	subcom_decompress->add_option("-r", "Compression ratio to use if decompressed size can't be determined");
	subcom_decompress->add_option("-o", "Output folder");
	subcom_decompress->add_flag("-y", "Overwrite without asking"); //TODO
	subcom_decompress->add_option_group("")->add_flag("--bench", "Benchmark mode");

	CLI11_PARSE(app, argc, argv);

	CLI::App* subcom = app.get_subcommands()[0];
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
		const bool BENCHMARK_MODE = subcom_compress->get_option_no_throw("--bench")->as<bool>();

		// if (std::filesystem::exists(INPUT) && std::filesystem::is_directory(OUTPUT_DIR)) {
		char* file_data;
		const int64_t file_size = q4b::LoadFileIntoMemory(INPUT, &file_data);
		if (file_size == -1) {
			std::cerr << "ERROR: file not found\n";
			return 1;
		}

		q4b::CompressionScheme scheme;
		int ret;
		ret = StrToScheme(SCHEME, &scheme);
		if (ret) { return 1; }

		if (scheme == q4b::CompressionScheme::Uncompressed) {
			std::cerr << "ERROR: file is uncompressed, nothing to do\n";
			return 1;
		}

		CompressionSchemeFunctions* functions = q4b::SchemeToFunctions(scheme);
		if (functions == nullptr) {
			std::cerr << "ERROR: unsupported scheme\n";
			return 1;
		}

		int level;
		if (scheme == q4b::CompressionScheme::zstd) {
			if (LEVEL == "max" || LEVEL == "--max" || LEVEL == "MAX") {
				level = INT_MAX;
			} else {
				level = std::stoi(LEVEL);
			}
		} else {
			level = std::stoi(LEVEL);
		}

		void* compressed_file_data;
		const q4b::Q4B_CompressionFileFlags flags = WRITE_METADATA ? q4b::Q4B_CompressionFileFlags::DoWriteMetadata : q4b::Q4B_CompressionFileFlags::None;
		auto timeStart = std::chrono::steady_clock::now();
		uint64_t compressedSize = functions->Compress_GenericExport(level, flags, file_data, file_size, &compressed_file_data);
		auto timeEnd = std::chrono::steady_clock::now();
		auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart); //TODO: microseconds?

		if (BENCHMARK_MODE) {
			std::cout << timeDiff << '\n'
			          << file_size << '\n' // Only outputting this to be ultra-robust
			          << compressedSize << '\n';
			if (!subcom_compress->get_option_no_throw("-o")->empty()) {
				// If -o was specified, it's to decompress in a later benchmark run
				std::string output = (OUTPUT_DIR / std::filesystem::path(INPUT).filename()).string() + SchemeToFileExt(scheme);
				WriteFile(output, (char*)compressed_file_data, compressedSize);
				std::cout << output;
			}
			// Don't bother cleaning up
		} else {
			delete functions;
			WriteFile((OUTPUT_DIR / std::filesystem::path(INPUT).filename()).string() + SchemeToFileExt(scheme), (char*)compressed_file_data, compressedSize);
			delete[] file_data;
			std::cout << "Compressed in " << timeDiff << std::endl;
		}

	} else if (subcom == subcom_decompress) {

		const std::string INPUT      = subcom_decompress->get_option_no_throw("input_file")->as<std::string>();
		const std::string SCHEME     = subcom_decompress->get_option_no_throw("scheme")->empty() ?
		                               "" : subcom_decompress->get_option_no_throw("scheme")->as<std::string>();
		const std::string RATIO      = subcom_decompress->get_option_no_throw("-r")->empty() ?
		                               "" : subcom_decompress->get_option_no_throw("-r")->as<std::string>();
		const std::string OUTPUT_DIR = subcom_decompress->get_option_no_throw("-o")->empty() ?
		                               "." : subcom_decompress->get_option_no_throw("-o")->as<std::string>();
		const bool BENCHMARK_MODE = subcom_decompress->get_option_no_throw("--bench")->as<bool>();

		char* file_data;
		const int64_t file_size = q4b::LoadFileIntoMemory(INPUT, &file_data);
		if (file_size == -1) {
			std::cerr << "ERROR: file not found\n";
			return 1;
		}

		q4b::CompressionScheme scheme;
		int ret;
		if (SCHEME == "") {
			ret = ExtToScheme(std::filesystem::path(INPUT).extension().string(), &scheme);
		} else {
			ret = StrToScheme(SCHEME, &scheme);
		}
		if (ret) { return 1; }

		if (scheme == q4b::CompressionScheme::Uncompressed) {
			std::cerr << "ERROR: file is uncompressed, nothing to do\n";
			return 1;
		}

		CompressionSchemeFunctions* functions = q4b::SchemeToFunctions(scheme);
		if (functions == nullptr) {
			std::cerr << "ERROR: unsupported scheme\n";
			return 1;
		}

		// float cratio;
		// if (RATIO == "") {
		// 	cratio = 0;
		// } else {
		// 	cratio = std::stof(RATIO);
		// }

		void* decompressed_file;
		auto timeStart = std::chrono::steady_clock::now();
		uint64_t decompressedSize = functions->Decompress_UnknownSize(file_data, file_size, &decompressed_file);
		auto timeEnd = std::chrono::steady_clock::now();
		auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart); //TODO: microseconds?

		if (BENCHMARK_MODE) {
			std::cout << timeDiff << '\n'
			          << file_size << '\n' // Only outputting this to be ultra-robust
			          << decompressedSize << '\n'
			          << q4b::CompressionToStr(scheme);
			// Don't bother cleaning up
		} else {
			delete functions;
			WriteFile(OUTPUT_DIR / std::filesystem::path(INPUT).stem(), (char*)decompressed_file, decompressedSize);
			delete[] file_data;
			std::cout << "Decompressed in " << timeDiff << std::endl;
		}

	} else {
		//oh no
	}

	return 0;
}
