#include "fileBrowse.h"
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <unistd.h>

#include "extract.hpp"
#include "inifile.h"
#include "graphic.h"
#include "ndsheaderbanner.h"
#include "pp2d/pp2d.h"

using namespace std;

int file_count = 0;

extern void displayBottomMsg(const char* text);

bool dirEntryPredicate(const DirEntry& lhs, const DirEntry& rhs)
{

	if (!lhs.isDirectory && rhs.isDirectory)
	{
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory)
	{
		return true;
	}
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

void findNdsFiles(vector<DirEntry>& dirContents)
{

	// dirContents.clear();

	// file_count = 0;
	
	struct stat st;
	DIR *pdir = opendir(".");

	if (pdir == NULL)
	{
		displayBottomMsg("Unable to open the directory.");
		for(int i=0;i<120;i++)
			gspWaitForVBlank();
	}
	else
	{
		while (true)
		{
			DirEntry dirEntry;

			struct dirent* pent = readdir(pdir);
			if (pent == NULL) break;

			stat(pent->d_name, &st);
			dirEntry.name = pent->d_name;
			dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;

			displayBottomMsg(dirEntry.name.c_str());
			// for(int i=0;i<30;i++)
			// 	gspWaitForVBlank();

			// if (showDirectories) {
				// if (dirEntry.name.compare(".") != 0 && dirEntry.name.compare("_nds") && dirEntry.name.compare("saves") != 0 && (dirEntry.isDirectory || strcasecmp(dirEntry.name.substr(dirEntry.name.length()-3, 3).c_str(), "nds"))) {
				// 	dirContents.push_back(dirEntry);
				// 	file_count++;
				// }
				if (strcasecmp(dirEntry.name.substr(dirEntry.name.length()-3, 3).c_str(), "nds") == 0) {
					// Get game's TID
					FILE *f_nds_file = fopen(dirEntry.name.c_str(), "rb");
					// char game_TID[5];
					grabTID(f_nds_file, dirEntry.tid);
					dirEntry.tid[4] = 0;
					fclose(f_nds_file);

					// dirEntry.tid = game_TID;

					dirContents.push_back(dirEntry);
					file_count++;
				} else if (dirEntry.isDirectory) {
					// char path[256];
					// getcwd(path, sizeof(path));
					// snprintf(path, sizeof(path), "%s%s", path, dirEntry.name.c_str());
					// displayBottomMsg(path);
					// for(int i=0;i<60;i++)
					// 		gspWaitForVBlank();
					chdir(dirEntry.name.c_str());
					findNdsFiles(dirContents);
					chdir("..");
				}
		}
		// sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);

		closedir(pdir);
	}
}
