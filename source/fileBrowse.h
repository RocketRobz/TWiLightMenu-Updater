#ifndef FILE_BROWSE_H
#define FILE_BROWSE_H

#include "download.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <vector>

using namespace std;

struct DirEntry
{
	string name;
	bool isDirectory;
	char tid[5];
};

void findNdsFiles(vector<DirEntry>& sdContents);

#endif //FILE_BROWSE_H
