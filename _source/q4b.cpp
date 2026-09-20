#include "q4b.hpp"

#include <algorithm>
#include <cstring> //memcpy

namespace q4b {

ArchiveHeader::ArchiveHeader() {
	std::memcpy(magic, MAGIC_NUM, sizeof(MAGIC_NUM));
	flags = 0;
	version = Q4B_ARCHIVE_VERSION;
	num_files = 0;
	self_hash = 0; // Exists to make sure the header isn't corrupted
}

void ArchiveHeader::computeHash() {
	self_hash = 0;
	self_hash = ComputeHash(this, sizeof(*this));
}

bool ArchiveHeader::verifyHash() const {
	ArchiveHeader copied(*this);
	copied.computeHash();
	return copied.self_hash == this->self_hash;
}

bool ArchivedFileHeader::pathIsValid() const {
	// No backslashes
	if (std::find(path, path + Q4B_MAX_PATH, '\\') != (path + Q4B_MAX_PATH)) {
		return false;
	}

	// It can be made into a string
	if (path[Q4B_MAX_PATH-1] != '\0') {
		return false;
	}

	// Rest of array is filled with zero
	uint32_t firstZero = std::find(path, path + Q4B_MAX_PATH, 0) - path;
	if (!std::all_of(path + firstZero, path + Q4B_MAX_PATH, [](auto val) { return val == 0; })) {
		return false;
	}

	return true;
};

void ArchivedFileHeader::setPath(const std::string& path) {
	const std::string p = path;
	size_t charCount = std::min(p.size(), size_t(Q4B_MAX_PATH-1));
	std::copy(p.begin(), p.begin() + charCount, this->path);
	std::replace(this->path, this->path + charCount, '\\', '/');
	std::fill(this->path + charCount, this->path + Q4B_MAX_PATH, '\0');
}

} // namespace q4b
