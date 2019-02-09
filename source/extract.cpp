#include "extract.hpp"

#include <archive.h>
#include <archive_entry.h>

Result extractArchive(std::string archivePath, std::string wantedFile, std::string outputPath)
{
	int r;
	struct archive_entry *entry;
	
	struct archive *a = archive_read_new();
	archive_read_support_format_all(a);
	archive_read_support_format_raw(a);
	
	r = archive_read_open_filename(a, archivePath.c_str(), 0x4000);
	if (r != ARCHIVE_OK) {
		return EXTRACT_ERROR_ARCHIVE;
	}
	
	Result ret = EXTRACT_ERROR_FIND;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		std::string entryName(archive_entry_pathname(entry));
		if (wantedFile == "/")	wantedFile = "";
		if (matchPattern(wantedFile, entryName.substr(0,wantedFile.length())) || wantedFile == "") {
			ret = EXTRACT_ERROR_NONE;

			Handle fileHandle;
			std::string outputPathFinal = outputPath + entryName.substr(wantedFile.length());
			if (outputPathFinal.substr(outputPathFinal.length()-1) == "/")	continue;
			Result res = openFile(&fileHandle, outputPathFinal.c_str(), true);
			if (R_FAILED(res)) {
				ret = EXTRACT_ERROR_OPENFILE;
				break;
			}
			
			u64 fileSize = archive_entry_size(entry);
			u32 toRead = 0x4000;
			u8 * buf = (u8 *)malloc(toRead);
			if (buf == NULL) {
				ret = EXTRACT_ERROR_ALLOC;
				FSFILE_Close(fileHandle);
				break;
			}
			
			u32 bytesWritten = 0;
			u64 offset = 0;
			do {
				if (toRead > fileSize) toRead = fileSize;
				ssize_t size = archive_read_data(a, buf, toRead);
				if (size < 0) {
					ret = EXTRACT_ERROR_READFILE;
					break;
				}
				
				res = FSFILE_Write(fileHandle, &bytesWritten, offset, buf, toRead, 0);
				if (R_FAILED(res)) {
					ret = EXTRACT_ERROR_WRITEFILE;
					break;
				}
				
				offset += bytesWritten;
				fileSize -= bytesWritten;
			} while(fileSize);
			
			FSFILE_Close(fileHandle);
			free(buf);
			// break;
		}
	}
	
	archive_read_free(a);
	
	return ret;
}