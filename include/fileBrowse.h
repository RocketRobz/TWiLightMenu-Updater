#ifndef FILE_BROWSE_H
#define FILE_BROWSE_H

#include "download.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <vector>

using namespace std;

struct DirEntry {
	string name;
	bool isDirectory;
	char tid[5];
	off_t size;
};

typedef struct {
	char gameTitle[12];			//!< 12 characters for the game title.
	char gameCode[4];			//!< 4 characters for the game code.
} sNDSHeadertitlecodeonly;

void findNdsFiles(vector<DirEntry>& dirContents);

void getDirectoryContents (vector<DirEntry>& dirContents);

#endif //FILE_BROWSE_H
