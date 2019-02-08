#pragma once

#include "common.hpp"

enum ExtractError {
	EXTRACT_ERROR_NONE = 0,
	EXTRACT_ERROR_ARCHIVE,
	EXTRACT_ERROR_ALLOC,
	EXTRACT_ERROR_FIND,
	EXTRACT_ERROR_READFILE,
	EXTRACT_ERROR_OPENFILE,
	EXTRACT_ERROR_WRITEFILE,
};

Result extractArchive(std::string archivePath, std::string wantedFile, std::string outputPath);
