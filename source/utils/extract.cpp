#include "extract.hpp"

#include <archive.h>
#include <archive_entry.h>

int filesExtracted = 0;
std::string extractingFile = "";

// That are our File Progressbar variable.
u64 extractSize = 0, writeOffset = 0;

Result extractArchive(std::string archivePath, std::string wantedFile, std::string outputPath) {
	extractSize = 0, writeOffset = 0, filesExtracted = 0;

	archive *a = archive_read_new();
	archive_entry *entry;
	int flags;

	/* Select which attributes we want to restore. */
	flags = ARCHIVE_EXTRACT_TIME;
	flags |= ARCHIVE_EXTRACT_PERM;
	flags |= ARCHIVE_EXTRACT_ACL;
	flags |= ARCHIVE_EXTRACT_FFLAGS;

	a = archive_read_new();
	archive_read_support_format_all(a);

	if(archive_read_open_filename(a, archivePath.c_str(), 0x4000) != ARCHIVE_OK) {
		return EXTRACT_ERROR_OPENFILE;
	}

	while(archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		if(archive_entry_size(entry) > 0) { // Ignore folders
			std::smatch match;
			std::string entryName(archive_entry_pathname(entry));
			if(std::regex_search(entryName, match, std::regex(wantedFile))) {
				extractingFile = outputPath + match.suffix().str();
				
				// make directories
				int substrPos = 1;
				while(extractingFile.find("/", substrPos)) {
					mkdir(extractingFile.substr(0, substrPos).c_str(), 0777);
					substrPos = extractingFile.find("/", substrPos) + 1;
				}

				uint sizeLeft = archive_entry_size(entry);
				extractSize = sizeLeft;
				writeOffset = 0;
				FILE *file = fopen(extractingFile.c_str(), "wb");
				if(!file) {
					return EXTRACT_ERROR_WRITEFILE;
				}

				u8 *buf = new u8[0x30000];
				if(buf == nullptr) {
					return EXTRACT_ERROR_ALLOC;
				}

				while(sizeLeft > 0) {
					u64 toRead = std::min(0x30000u, sizeLeft);
					ssize_t size = archive_read_data(a, buf, toRead);
					fwrite(buf, 1, size, file);
					sizeLeft -= size;
					writeOffset += size;
				}
				filesExtracted++;
				fclose(file);
				delete[] buf;
			}
		}
	}

	archive_read_close(a);
	archive_read_free(a);
	return EXTRACT_ERROR_NONE;
}