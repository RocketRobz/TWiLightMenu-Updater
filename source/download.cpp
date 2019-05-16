#include "download.hpp"
#include <sys/stat.h>
#include <vector>
#include <unistd.h>

#include "extract.hpp"
#include "inifile.h"
#include "graphic.h"
#include "fileBrowse.h"
#include "pp2d/pp2d.h"
#include "thread.h"

extern "C" {
	#include "cia.h"
}

#define  USER_AGENT   APP_TITLE "-" VERSION_STRING

static char* result_buf = NULL;
static size_t result_sz = 0;
static size_t result_written = 0;
std::vector<std::string> _topText;
std::string jsonName;
CIniFile versionsFile("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersionsV2.ini");
std::string latestMenuReleaseCache = "";
std::string latestMenuNightlyCache = "";
std::string latestBootstrapReleaseCache = "";
std::string latestBootstrapNightlyCache = "";
std::string latestUpdaterReleaseCache = "";
std::string latestUpdaterNightlyCache = "";

extern void displayBottomMsg(const char* text);

extern bool downloadNightlies;
extern bool updateAvailable[];
extern bool updated3dsx;
extern int filesExtracted;
extern std::string extractingFile;

char progressBarMsg[64] = "";
bool showProgressBar = false;
bool progressBarType = 0; // 0 = Download | 1 = Extract
bool continueNdsScan = true;

// following function is from 
// https://github.com/angelsl/libctrfgh/blob/master/curl_test/src/main.c
static size_t handle_data(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	(void) userdata;
	const size_t bsz = size*nmemb;

	if (result_sz == 0 || !result_buf)
	{
		result_sz = 0x1000;
		result_buf = (char*)malloc(result_sz);
	}

	bool need_realloc = false;
	while (result_written + bsz > result_sz) 
	{
		result_sz <<= 1;
		need_realloc = true;
	}

	if (need_realloc)
	{
		char *new_buf = (char*)realloc(result_buf, result_sz);
		if (!new_buf)
		{
			return 0;
		}
		result_buf = new_buf;
	}

	if (!result_buf)
	{
		return 0;
	}

	memcpy(result_buf + result_written, ptr, bsz);
	result_written += bsz;
	return bsz;
}

static Result setupContext(CURL *hnd, const char * url)
{
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_URL, url);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, handle_data);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(hnd, CURLOPT_STDERR, stdout);

	return 0;
}

Result downloadToFile(std::string url, std::string path)
{
	Result ret = 0;	
	printf("Downloading from:\n%s\nto:\n%s\n", url.c_str(), path.c_str());

	void *socubuf = memalign(0x1000, 0x100000);
	if (!socubuf)
	{
		return -1;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(ret))
	{
		free(socubuf);
		return ret;
	}

	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, url.c_str());
	if (ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return ret;
	}

	Handle fileHandle;
	u64 offset = 0;
	u32 bytesWritten = 0;
	
	ret = openFile(&fileHandle, path.c_str(), true);
	if (R_FAILED(ret)) {
		printf("Error: couldn't open file to write.\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return DL_ERROR_WRITEFILE;
	}
	
	u64 startTime = osGetTime();

	CURLcode cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);

	if (cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return -1;
	}
	
	FSFILE_Write(fileHandle, &bytesWritten, offset, result_buf, result_written, 0);

	u64 endTime = osGetTime();
	u64 totalTime = endTime - startTime;
	printf("Download took %llu milliseconds.\n", totalTime);

	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;
	FSFILE_Close(fileHandle);
	return 0;
}

Result downloadFromRelease(std::string url, std::string asset, std::string path)
{
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if (!socubuf)
	{
		return -1;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(ret))
	{
		free(socubuf);
		return ret;
	}

	std::regex parseUrl("github\\.com\\/(.+)\\/(.+)");
	std::smatch result;
	regex_search(url, result, parseUrl);
	
	std::string repoOwner = result[1].str(), repoName = result[2].str();
	
	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repoOwner << "/" << repoName << "/releases/latest";
	std::string apiurl = apiurlStream.str();
	
	printf("Downloading latest release from repo:\n%s\nby:\n%s\n", repoName.c_str(), repoOwner.c_str());
	printf("Crafted API url:\n%s\n", apiurl.c_str());
	
	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if (ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return ret;
	}

	CURLcode cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string
	
	if (cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return -1;
	}
	
	printf("Looking for asset with name matching:\n%s\n", asset.c_str());
	std::string assetUrl;
	json parsedAPI = json::parse(result_buf);
	if (parsedAPI["assets"].is_array()) {
		for (auto jsonAsset : parsedAPI["assets"]) {
			if (jsonAsset.is_object() && jsonAsset["name"].is_string() && jsonAsset["browser_download_url"].is_string()) {
				std::string assetName = jsonAsset["name"];
				if (matchPattern(asset, assetName)) {
					assetUrl = jsonAsset["browser_download_url"];
					break;
				}
			}
		}
	}
	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;
	
	if (assetUrl.empty())
		ret = DL_ERROR_GIT;
	else
		ret = downloadToFile(assetUrl, path);
	
	return ret;
}

/**
 * Check Wi-Fi status.
 * @return True if Wi-Fi is connected; false if not.
 */
bool checkWifiStatus(void) {
	u32 wifiStatus;
	bool res = false;

	if (R_SUCCEEDED(ACU_GetWifiStatus(&wifiStatus)) && wifiStatus) {
		res = true;
	}
	
	return res;
}

void downloadFailed(void) {
	displayBottomMsg("Download failed!");
	for (int i = 0; i < 60*2; i++) {
		gspWaitForVBlank();
	}
}

void notConnectedMsg(void) {
	displayBottomMsg("Please connect to Wi-Fi");
	for (int i = 0; i < 60*2; i++) {
		gspWaitForVBlank();
	}
}

void doneMsg(void) {
	displayBottomMsg("Done!");
	for (int i = 0; i < 60*2; i++) {
		gspWaitForVBlank();
	}
}

std::string getLatestRelease(std::string repo, std::string item)
{
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if (!socubuf)
	{
		return "";
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(ret))
	{
		free(socubuf);
		return "";
	}
	
	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/releases/latest";
	std::string apiurl = apiurlStream.str();
	
	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if (ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}

	CURLcode cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string
	
	if (cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}
	
	std::string jsonItem;
	json parsedAPI = json::parse(result_buf);
	if (parsedAPI[item].is_string()) {
		jsonItem = parsedAPI[item];
	}
	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;

	return jsonItem;
}

std::string getLatestCommit(std::string repo, std::string item)
{
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if (!socubuf)
	{
		return "";
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(ret))
	{
		free(socubuf);
		return "";
	}
	
	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/commits/master";
	std::string apiurl = apiurlStream.str();
	
	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if (ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}

	CURLcode cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string
	
	if (cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}
	
	std::string jsonItem;
	json parsedAPI = json::parse(result_buf);
	if (parsedAPI[item].is_string()) {
		jsonItem = parsedAPI[item];
	}
	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;

	return jsonItem;
}

std::string getLatestCommit(std::string repo, std::string array, std::string item)
{
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if (!socubuf)
	{
		return "";
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(ret))
	{
		free(socubuf);
		return "";
	}
	
	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/commits/master";
	std::string apiurl = apiurlStream.str();
	
	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if (ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}

	CURLcode cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string
	
	if (cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}
	
	std::string jsonItem;
	json parsedAPI = json::parse(result_buf);
	if (parsedAPI[array][item].is_string()) {
		jsonItem = parsedAPI[array][item];
	}
	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;

	return jsonItem;
}

std::vector<ThemeEntry> getThemeList(std::string repo, std::string path)
{
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	std::vector<ThemeEntry> emptyVector;
	if (!socubuf)
	{
		return emptyVector;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(ret))
	{
		free(socubuf);
		return emptyVector;
	}
	
	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/contents/" << path;
	std::string apiurl = apiurlStream.str();
	
	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if (ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return emptyVector;
	}

	CURLcode cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string
	
	if (cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		
		return emptyVector;
	}
	
	std::vector<ThemeEntry> jsonItems;
	json parsedAPI = json::parse(result_buf);
	for(uint i=0;i<parsedAPI.size();i++) {
		ThemeEntry themeEntry;
		if (parsedAPI[i]["name"].is_string()) {
			themeEntry.name = parsedAPI[i]["name"];
		}
		if (parsedAPI[i]["download_url"].is_string()) {
			themeEntry.downloadUrl = parsedAPI[i]["download_url"];
		}
		if (parsedAPI[i]["path"].is_string()) {
			themeEntry.sdPath = "sdmc:/";
			themeEntry.sdPath += parsedAPI[i]["path"];
			themeEntry.path = parsedAPI[i]["path"];

			size_t pos;
			while ((pos = themeEntry.path.find(" ")) != std::string::npos) {
				themeEntry.path.replace(pos, 1, "%20");
			}
		}
		jsonItems.push_back(themeEntry);
	}
	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;

	return jsonItems;
}

void downloadTheme(std::string path) {
	vector<ThemeEntry> themeContents = getThemeList("DS-Homebrew/twlmenu-extras", path);
	for(uint i=0;i<themeContents.size();i++) {
		if(themeContents[i].downloadUrl != "") {
			displayBottomMsg(("Downloading: "+themeContents[i].name).c_str());
			downloadToFile(themeContents[i].downloadUrl, themeContents[i].sdPath);
		} else {
			displayBottomMsg(("Downloading: "+themeContents[i].name).c_str());
			mkdir((themeContents[i].sdPath).c_str(), 0777);
			downloadTheme(themeContents[i].path);
		}
	}
}

bool showReleaseInfo(std::string repo, bool showExitText)
{
	jsonName = getLatestRelease(repo, "name");
	std::string jsonBody = getLatestRelease(repo, "body");
	
	setMessageText(jsonBody);
	int textPosition = 0;
	bool redrawText = true;

	while(1) {
		if(redrawText) {
			drawMessageText(textPosition, showExitText);
			redrawText = false;
		}

		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();

		if(hHeld & KEY_UP || hHeld & KEY_DOWN) {
			for(int i=0;i<9;i++)
				gspWaitForVBlank();
		}
		
		if (hDown & KEY_A || hDown & KEY_Y || hDown & KEY_TOUCH) {
			return true;
		} else if (hDown & KEY_B) {
			return false;
		} else if (hHeld & KEY_UP) {
			if(textPosition > 0) {
				textPosition--;
				redrawText = true;
			}
		} else if (hHeld & KEY_DOWN) {
			if(textPosition < (int)(_topText.size() - 10)) {
				textPosition++;
				redrawText = true;
			}
		}
	}
}

void showCommitInfo(std::string repo)
{
	jsonName = getLatestCommit(repo, "sha").substr(0,7);
	std::string jsonBody = getLatestCommit(repo, "commit", "message");
	setMessageText(jsonBody);
	int textPosition = 0;
	bool redrawText = true;

	while(1) {
		if(redrawText) {
			drawMessageText(textPosition, false);
			redrawText = false;
		}

		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();

		if(hHeld & KEY_UP || hHeld & KEY_DOWN) {
			for(int i=0;i<9;i++)
				gspWaitForVBlank();
		}
		
		if (hDown & KEY_A || hDown & KEY_B || hDown & KEY_Y) {
			break;
		} else if (hHeld & KEY_UP) {
			if(textPosition > 0) {
				textPosition--;
				redrawText = true;
			}
		} else if (hHeld & KEY_DOWN) {
			if(textPosition < (int)(_topText.size() - 10)) {
				textPosition++;
				redrawText = true;
			}
		}
	}
}

void setMessageText(const std::string &text)
{
	std::string _topTextStr(text);
	std::vector<std::string> words;
	std::size_t pos;
	// std::replace( _topTextStr.begin(), _topTextStr.end(), '\n', ' ');
	_topTextStr.erase(std::remove(_topTextStr.begin(), _topTextStr.end(), '\r'), _topTextStr.end());
	while((pos = _topTextStr.find(' ')) != std::string::npos) {
		words.push_back(_topTextStr.substr(0, pos));
		_topTextStr = _topTextStr.substr(pos + 1);
	}
	if(_topTextStr.size())
		words.push_back(_topTextStr);
	std::string temp;
	_topText.clear();
	for(auto word : words)
	{
		int width = pp2d_get_text_width((temp + " " + word).c_str(), 0.5f, 0.5f);
		if(word.find('\n') != -1u)
		{
			word.erase(std::remove(word.begin(), word.end(), '\n'), word.end());
			temp += " " + word;
			_topText.push_back(temp);
			temp = "";
		}
		else if(width > 256)
		{
			_topText.push_back(temp);
			temp = word;
		}
		else
		{
			temp += " " + word;
		}
	}
	if(temp.size())
	   _topText.push_back(temp);
}

void drawMessageText(int position, bool showExitText)
{
	pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
	pp2d_draw_texture(loadingbgtex, 0, 0);
	pp2d_draw_text(18, 24, .7, .7, BLACK, jsonName.c_str());
	for (int i = 0; i < (int)_topText.size() && i < (showExitText ? 9 : 10); i++) {
			pp2d_draw_text(24, ((i * 16) + 48), 0.5f, 0.5f, BLACK, _topText[i+position].c_str());
	}
	if(showExitText)
		pp2d_draw_text(24, 200, 0.5f, 0.5f, BLACK, "B: Cancel   A: Update");
	pp2d_end_draw();
}

void displayProgressBar() {
	char str[256];
	while(showProgressBar) {
		snprintf(str, sizeof(str), "%s\n%s%s\n%i %s", progressBarMsg, (!progressBarType ? "" : "\nCurrently extracting:\n"), (!progressBarType ? "" : extractingFile.c_str()), (!progressBarType ? (int)round(result_written/1000) : filesExtracted), (!progressBarType ? "KB downloaded." : (filesExtracted == 1 ? "file extracted." :"files extracted.")));
		displayBottomMsg(str);
		gspWaitForVBlank();
	}
}

std::string latestMenuRelease(void) {
	if (latestMenuReleaseCache == "")
		latestMenuReleaseCache = getLatestRelease("DS-Homebrew/TWiLightMenu", "tag_name");
	return latestMenuReleaseCache;
}

std::string latestMenuNightly(void) {
	if (latestMenuNightlyCache == "")
		latestMenuNightlyCache = getLatestCommit("DS-Homebrew/TWiLightMenu", "sha").substr(0,7);
	return latestMenuNightlyCache;
}

std::string latestBootstrapRelease(void) {
	if (latestBootstrapReleaseCache == "")
		latestBootstrapReleaseCache = getLatestRelease("ahezard/nds-bootstrap", "tag_name");
	return latestBootstrapReleaseCache;
}

std::string latestBootstrapNightly(void) {
	if (latestBootstrapNightlyCache == "")
		latestBootstrapNightlyCache = getLatestCommit("ahezard/nds-bootstrap", "sha").substr(0,7);
	return latestBootstrapNightlyCache;
}

std::string latestUpdaterRelease(void) {
	if (latestUpdaterReleaseCache == "")
		latestUpdaterReleaseCache = getLatestRelease("RocketRobz/TWiLightMenu-Updater", "tag_name");
	return latestUpdaterReleaseCache;
}

std::string latestUpdaterNightly(void) {
	if (latestUpdaterNightlyCache == "")
		latestUpdaterNightlyCache = getLatestCommit("RocketRobz/TWiLightMenu-Updater", "sha").substr(0,7);
	return latestUpdaterNightlyCache;
}

void saveUpdateData(void) {
	versionsFile.SaveIniFile("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersionsV2.ini");
}

std::string getInstalledVersion(std::string component) {
	return versionsFile.GetString(component, "VERSION", "");
}

std::string getInstalledChannel(std::string component) {
	return versionsFile.GetString(component, "CHANNEL", "");
}

void setInstalledVersion(std::string component, std::string version) {
	versionsFile.SetString(component, "VERSION", version);
}

void setInstalledChannel(std::string component, std::string channel) {
	versionsFile.SetString(component, "CHANNEL", channel);
}

void checkForUpdates() {
	// First remove the old versions file
	unlink("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");

	std::string menuChannel = getInstalledChannel("TWILIGHTMENU");
	std::string menuVersion = getInstalledVersion("TWILIGHTMENU");
	std::string updaterChannel = getInstalledChannel("TWILIGHTMENU-UPDATER");
	std::string updaterVersion = getInstalledVersion("TWILIGHTMENU-UPDATER");
	std::string bootstrapRelease = getInstalledVersion("NDS-BOOTSTRAP-RELEASE");
	std::string boostrapNightly = getInstalledVersion("NDS-BOOTSTRAP-NIGHTLY");

	if (menuChannel == "release")
		updateAvailable[0] = menuVersion != latestMenuRelease();
	else if (menuChannel == "nightly")
		updateAvailable[1] = menuVersion != latestMenuNightly();
	else
		updateAvailable[0] = updateAvailable[1] = true;

	updateAvailable[2] = bootstrapRelease != latestBootstrapRelease();
	updateAvailable[3] = boostrapNightly != latestBootstrapNightly();

	if (updaterChannel == "release")
		updateAvailable[4] = updaterVersion != latestUpdaterRelease();
	else if (updaterChannel == "nightly")
		updateAvailable[5] = updaterVersion != latestUpdaterNightly();
	else
		updateAvailable[4] = updateAvailable[5] = true;
}

void updateBootstrap(bool nightly) {
	if(nightly) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading nds-bootstrap...\n(Nightly)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/nds-bootstrap.7z?raw=true", "/nds-bootstrap-nightly.7z") != 0) {
			showProgressBar = false;
			downloadFailed();
			return;
		}

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting nds-bootstrap...\n(Nightly)");
		filesExtracted = 0;
		progressBarType = 1;
		extractArchive("/nds-bootstrap-nightly.7z", "nds-bootstrap/", "/_nds/");
		showProgressBar = false;

		deleteFile("sdmc:/nds-bootstrap-nightly.7z");

		setInstalledVersion("NDS-BOOTSTRAP-NIGHTLY", latestBootstrapNightly());
		saveUpdateData();
		updateAvailable[3] = false;
	} else {	
		displayBottomMsg("Downloading nds-bootstrap...\n(Release)");
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading nds-bootstrap...\n(Release)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadFromRelease("https://github.com/ahezard/nds-bootstrap", "nds-bootstrap\\.zip", "/nds-bootstrap-release.zip") != 0) {
			showProgressBar = false;
			downloadFailed();
			return;
		}

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting nds-bootstrap...\n(Release)");
		filesExtracted = 0;
		progressBarType = 1;
		extractArchive("/nds-bootstrap-release.zip", "/", "/_nds/");
		showProgressBar = false;

		deleteFile("sdmc:/nds-bootstrap-release.zip");

		setInstalledVersion("NDS-BOOTSTRAP-RELEASE", latestBootstrapRelease());
		saveUpdateData();
		updateAvailable[2] = false;
	}
	doneMsg();
}

void updateTWiLight(bool nightly) {
	if(nightly) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++...\n(Nightly)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/TWiLightMenu.7z?raw=true", "/TWiLightMenu-nightly.7z") != 0) {
			showProgressBar = false;
			downloadFailed();
			return;
		}

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting TWiLight Menu++...\n(Nightly)");
		filesExtracted = 0;
		progressBarType = 1;
		extractArchive("/TWiLightMenu-nightly.7z", "TWiLightMenu/_nds/", "/_nds/");
		extractArchive("/TWiLightMenu-nightly.7z", "3DS - CFW users/", "/");
		showProgressBar = false;

		displayBottomMsg("Installing TWiLight Menu++ CIA...\n"
						"(Nightly)");
		// installCia("/TWiLight Menu.cia");
		installCia("/TWiLight Menu - Game booter.cia");

		deleteFile("sdmc:/TWiLightMenu-nightly.7z");
		deleteFile("sdmc:/TWiLight Menu.cia");
		deleteFile("sdmc:/TWiLight Menu - Game booter.cia");

		setInstalledChannel("TWILIGHTMENU", "nightly");
		setInstalledVersion("TWILIGHTMENU", latestMenuNightly());
		saveUpdateData();
		updateAvailable[1] = false;
	} else {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++...\n(Release)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadFromRelease("https://github.com/DS-Homebrew/TWiLightMenu", "TWiLightMenu\\.7z", "/TWiLightMenu-release.7z") != 0) {
			showProgressBar = false;
			downloadFailed();
			return;
		}

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting TWiLight Menu++...\n(Release)");
		filesExtracted = 0;
		progressBarType = 1;
		extractArchive("/TWiLightMenu-release.7z", "_nds/", "/_nds/");
		extractArchive("/TWiLightMenu-release.7z", "3DS - CFW users/", "/");
		extractArchive("/TWiLightMenu-release.7z", "DSi&3DS - SD card users/", "/");
		showProgressBar = false;

		displayBottomMsg("Installing TWiLight Menu++ CIA...\n"
						"(Release)");
		installCia("/TWiLight Menu.cia");
		installCia("/TWiLight Menu - Game booter.cia");

		deleteFile("sdmc:/TWiLightMenu-release.7z");
		deleteFile("sdmc:/TWiLight Menu.cia");
		deleteFile("sdmc:/TWiLight Menu - Game booter.cia");

		setInstalledChannel("TWILIGHTMENU", "release");
		setInstalledVersion("TWILIGHTMENU", latestMenuRelease());
		saveUpdateData();
		updateAvailable[0] = false;
	}
	doneMsg();
}

void updateSelf(bool nightly) {
	if(nightly && (access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) != 0)) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++ Updater...\n(Nightly (cia))");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/TWiLightMenu%20Updater/TWiLight_Menu++_Updater.cia?raw=true", "/TWiLightMenu-Updater-nightly.cia") != 0) {
			downloadFailed();
			return;
		}
		showProgressBar = false;

		// Before the cia install because rebooting the app might've been causing this file to get cleared
		setInstalledChannel("TWILIGHTMENU-UPDATER", "nightly");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterNightly());
		saveUpdateData();
		updateAvailable[5] = false;

		displayBottomMsg("Installing TWiLight Menu++ Updater CIA...\n"
						"(Nightly)\n\n\n\n\n\n\n\n\n\n"
						"The app will reboot when done.");
		installCia("/TWiLightMenu-Updater-nightly.cia");

		deleteFile("sdmc:/TWiLightMenu-Updater-nightly.cia");
	} else if(!nightly && (access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) != 0)) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++ Updater...\n(Release (cia))");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadFromRelease("https://github.com/RocketRobz/TWiLightMenu-Updater", "TWiLightMenu-Updater\\.cia", "/TWiLightMenu-Updater-release.cia") != 0) {
			downloadFailed();
			return;
		}
		showProgressBar = false;

		// Before the cia install because rebooting the app might've been causing this file to get cleared
		setInstalledChannel("TWILIGHTMENU-UPDATER", "release");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterRelease());
		saveUpdateData();
		updateAvailable[4] = false;

		displayBottomMsg("Installing TWiLight Menu++ Updater CIA...\n"
						"(Release)\n\n\n\n\n\n\n\n\n\n"
						"The app will reboot when done.");
		installCia("/TWiLightMenu-Updater-release.cia");

		deleteFile("sdmc:/TWiLightMenu-Updater-release.cia");
	} else if(nightly && (access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) == 0)) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++ Updater...\n(Nightly (3dsx))");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/TWiLightMenu%20Updater/TWiLight_Menu++_Updater.3dsx?raw=true", "/3ds/TWiLightMenu-Updater.3dsx") != 0) {
			downloadFailed();
			return;
		}
		showProgressBar = false;

		setInstalledChannel("TWILIGHTMENU-UPDATER", "nightly");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterNightly());
		saveUpdateData();
		updateAvailable[5] = false;
	} else {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++ Updater...\n(Release (3dsx))");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if (downloadFromRelease("https://github.com/RocketRobz/TWiLightMenu-Updater", "TWiLightMenu-Updater\\.3dsx", "/3ds/TWiLightMenu-Updater.3dsx") != 0) {
			downloadFailed();
			return;
		}
		showProgressBar = false;

		setInstalledChannel("TWILIGHTMENU-UPDATER", "release");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterRelease());
		saveUpdateData();
		updateAvailable[4] = false;
	}
	doneMsg();
	if(access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) == 0) {
		updated3dsx = true;
	}
}

void updateCheats(void) {
	snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading DSJ's cheat database...");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
	if (downloadToFile("https://github.com/TWLBot/Builds/raw/master/usrcheat.dat.7z", "/usrcheat.dat.7z") != 0) {
		downloadFailed();
		return;
	}

	snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting DSJ's cheat database...");
	filesExtracted = 0;
	progressBarType = 1;
	extractArchive("/usrcheat.dat.7z", "usrcheat.dat", "/_nds/TWiLightMenu/extras/usrcheat.dat");
	showProgressBar = false;

	deleteFile("sdmc:/usrcheat.dat.7z");

	doneMsg();
}

const char* getBoxartRegion(char tid_region) {
	// European boxart languages.
	static const char *const ba_langs_eur[] = {
		"EN",	// Japanese (English used in place)
		"EN",	// English
		"FR",	// French
		"DE",	// German
		"IT",	// Italian
		"ES",	// Spanish
		"ZHCN",	// Simplified Chinese
		"KO",	// Korean
	};
	CIniFile ini("sdmc:/_nds/TWiLightMenu/settings.ini");
	int language = ini.GetInt("SRLOADER", "LANGUAGE", -1);
	const char *ba_region;

	switch (tid_region) {
		case 'E':
		case 'T':
			ba_region = "US";	// USA
			break;
		case 'J':
			ba_region = "JA";	// Japanese
			break;
		case 'K':
			ba_region = "KO";	// Korean
			break;

		case 'O':			// USA/Europe
			// if (region == CFG_REGION_USA) {
				// System is USA region.
				// Get the USA boxart if it's available.
				ba_region = "EN";
				break;
			// }
			// fall-through
		case 'P':			// Europe
		default:
			// System is not USA region.
			// Get the European boxart that matches the system's language.
			// TODO: Check country code for Australia.
			// This requires parsing the Config savegame. (GetConfigInfoBlk2())
			// Reference: https://3dbrew.org/wiki/Config_Savegame
			if(language == -1)
				ba_region = "EN";	// TODO: make this actually set to the console's language
			else
				ba_region = ba_langs_eur[language];
			break;

		case 'U':
			// Alternate country code for Australia.
			ba_region = "AU";
			break;

		// European country-specific localizations.
		case 'D':
			ba_region = "DE";	// German
			break;
		case 'F':
			ba_region = "FR";	// French
			break;
		case 'H':
			ba_region = "NL";	// Dutch
			break;
		case 'I':
			ba_region = "IT";	// Italian
			break;
		case 'R':
			ba_region = "RU";	// Russian
			break;
		case 'S':
			ba_region = "ES";	// Spanish
			break;
		case '#':
			ba_region = "HB"; // Homebrew
			break;
	}
	return ba_region;
}

void scanToCancelBoxArt(void) {
	while(continueNdsScan) {
		hidScanInput();
		if(hidKeysDown() & KEY_B) {
			continueNdsScan = false;
		}
		gspWaitForVBlank();
	}
}

void downloadArt(void) {
	std::string artOptions[] = {"Boxart", "Themes"};
	int selectedOption = 0;
	while(1) {
		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();
		if(hDown & KEY_A) {
			if(selectedOption == 0) {
				downloadBoxart();
			} else {
				downloadThemes();
			}
		} else if(hDown & KEY_B) {
			return;
		} else if(hDown & KEY_UP) {
			if(selectedOption > 0) {
				selectedOption--;
			}
		} else if(hDown & KEY_DOWN) {
			if(selectedOption < 1) {
				selectedOption++;
			}
		}
		std::string artText = "Would you like to download boxart\nor themes?\n";
		for(int i=0;i<2;i++) {
			if(i == selectedOption) {
				artText += "> " + artOptions[i] + "\n";
			} else {
				artText += "  " + artOptions[i] + "\n";
			}
		}
		artText += "\n\n\n\n\n\n\n";
		artText += "B: Back   A: Choose";
		displayBottomMsg(artText.c_str());
	}
}

void downloadBoxart(void) {

	vector<DirEntry> dirContents;
	std::string scanDir;

	displayBottomMsg("Would you like to choose a directory, or scan\nthe full card?\n\n\n\n\n\n\n\n\n\nB: Cancel   A: Choose Directory   X: Full SD");

	while(1) {
		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();

		if(hDown & KEY_A) {
			chdir("sdmc:/");
			bool dirChosen = false;
			uint selectedDir = 0;
			uint keyRepeatDelay = 0;
			while(!dirChosen) {
				getDirectoryContents(dirContents);
				for(uint i=0;i<dirContents.size();i++) {
					if(!dirContents[i].isDirectory) {
						dirContents.erase(dirContents.begin()+i);
					}
				}
				while(1) {
					gspWaitForVBlank();
					hidScanInput();
					const u32 hDown = hidKeysDown();
					const u32 hHeld = hidKeysHeld();
					if(keyRepeatDelay)	keyRepeatDelay--;
					if(hDown & KEY_A) {
						chdir(dirContents[selectedDir].name.c_str());
						selectedDir = 0;
						break;
					} else if(hDown & KEY_B) {
						char path[PATH_MAX];
						getcwd(path, PATH_MAX);
						if(strcmp(path, "sdmc:/") == 0)	return;
						chdir("..");
						selectedDir = 0;
						break;
					}	else if(hDown & KEY_X) {
						chdir(dirContents[selectedDir].name.c_str());
						char path[1024];
						getcwd(path, sizeof(path));
						scanDir = path;
						dirChosen = true;
						break;
					} else if(hHeld & KEY_UP && !keyRepeatDelay) {
						if(selectedDir > 0) {
							selectedDir--;
							keyRepeatDelay = 3;
						}
					} else if(hHeld & KEY_DOWN && !keyRepeatDelay) {
						if(selectedDir < dirContents.size()-1) {
							selectedDir++;
							keyRepeatDelay = 3;
						}
					} else if(hHeld & KEY_LEFT && !keyRepeatDelay) {
						selectedDir -= 10;
						if(selectedDir < 0) {
							selectedDir = 0;
						}
						keyRepeatDelay = 3;
					} else if(hHeld & KEY_RIGHT && !keyRepeatDelay) {
						selectedDir += 10;
						if(selectedDir > dirContents.size()) {
							selectedDir = dirContents.size()-1;
						}
						keyRepeatDelay = 3;
					}
					std::string dirs;
					for(uint i=(selectedDir<10) ? 0 : selectedDir-10;i<dirContents.size()&&i<((selectedDir<10) ? 11 : selectedDir+1);i++) {
						if(i == selectedDir) {
							dirs += "> " + dirContents[i].name + "\n";
						} else {
							dirs += "  " + dirContents[i].name + "\n";
						}
					}
					for(uint i=0;i<((dirContents.size()<10) ? 11-dirContents.size() : 0);i++) {
						dirs += "\n";
					}
					dirs += "B: Back   A: Open   X: Choose";
					displayBottomMsg(dirs.c_str());
				}
			}
			break;
		} else if(hDown & KEY_B) {
			return;
		} else if(hDown & KEY_X) {
			scanDir = "sdmc:/";
			break;
		}
	}

	displayBottomMsg("Scanning SD card for DS roms...\n\n(Press B to cancel)");

	chdir(scanDir.c_str());
	continueNdsScan = true;
	createThread((ThreadFunc)scanToCancelBoxArt);
	findNdsFiles(dirContents);
	continueNdsScan = false;

	for(int i=0;i<(int)dirContents.size();i++) {
		char downloadMessage[50];
		snprintf(downloadMessage, sizeof(downloadMessage), "Downloading \"%s.bmp\"...\n", dirContents[i].tid);
		displayBottomMsg(downloadMessage);

		const char *ba_region = getBoxartRegion(dirContents[i].tid[3]);
		
		char boxartUrl[256];
		snprintf(boxartUrl, sizeof(boxartUrl), "https://art.gametdb.com/ds/coverDS/%s/%s.bmp", ba_region, dirContents[i].tid);
		char boxartPath[256];
		snprintf(boxartPath, sizeof(boxartPath), "/_nds/TWiLightMenu/boxart/%s.bmp", dirContents[i].tid);
		
		downloadToFile(boxartUrl, boxartPath);
	}

	chdir("sdmc:/_nds/TWiLightMenu/boxart/");
	getDirectoryContents(dirContents);

	displayBottomMsg("Deleting blank files...");
	for(int i=0;i<(int)dirContents.size();i++) {
		if(dirContents[i].size == 0) {
			char path[256];
			snprintf(path, sizeof(path), "%s%s", "sdmc:/_nds/TWiLightMenu/boxart/", dirContents[i].name.c_str());
			deleteFile(path);
		}
	}
	doneMsg();
}

void downloadThemes(void) {

	int selectedTwlTheme = 0;
	int keyRepeatDelay = 0;
	std::string themeNames[] = {"DSi theme", "3DS theme", "R4 theme", "Acekard theme"};
	std::string themeFolders[] = {"dsimenu", "3dsmenu", "r4menu", "akmenu"};
	chooseTWlTheme:
	while(1) {
		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();
		if(keyRepeatDelay)	keyRepeatDelay--;
		if(hDown & KEY_A) {
			break;
		} else if(hDown & KEY_B) {
			return;
		} else if(hHeld & KEY_UP && !keyRepeatDelay) {
			if(selectedTwlTheme > 0) {
				selectedTwlTheme--;
				keyRepeatDelay = 3;
			}
		} else if(hHeld & KEY_DOWN && !keyRepeatDelay) {
			if(selectedTwlTheme < 3) {
				selectedTwlTheme++;
				keyRepeatDelay = 3;
			}
		}
		std::string themesText = "Which TWiLight theme would you like\nto download themes for?\n";
		for(int i=0;i<4;i++) {
			if(i == selectedTwlTheme) {
				themesText += "> " + themeNames[i] + "\n";
			} else {
				themesText += "  " + themeNames[i] + "\n";
			}
		}
		themesText += "\n\n\n\n\n";
		themesText += "B: Back   A: Choose";
		displayBottomMsg(themesText.c_str());
	}

	displayBottomMsg("Getting theme list...");

	std::vector<ThemeEntry> themeList;
	themeList = getThemeList("DS-Homebrew/twlmenu-extras", "_nds/TWiLightMenu/"+themeFolders[selectedTwlTheme]+"/themes");
	mkdir(("sdmc:/_nds/TWiLightMenu/"+themeFolders[selectedTwlTheme]).c_str(), 0777);
	mkdir(("sdmc:/_nds/TWiLightMenu/"+themeFolders[selectedTwlTheme]+"/themes/").c_str(), 0777);

	int selectedTheme = 0;
	while(1) {
		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();
		if(keyRepeatDelay)	keyRepeatDelay--;
		if(hDown & KEY_A) {
			mkdir((themeList[selectedTheme].sdPath).c_str(), 0777);
			displayBottomMsg(("Downloading: "+themeList[selectedTheme].name).c_str());
			downloadTheme(themeList[selectedTheme].path);
		} else if(hDown & KEY_B) {
			selectedTheme = 0;
			goto chooseTWlTheme;
		} else if(hHeld & KEY_UP && !keyRepeatDelay) {
			if(selectedTheme > 0) {
				selectedTheme--;
				keyRepeatDelay = 3;
			}
		} else if(hHeld & KEY_DOWN && !keyRepeatDelay) {
			if(selectedTheme < (int)themeList.size()-1) {
				selectedTheme++;
				keyRepeatDelay = 3;
			}
		} else if(hHeld & KEY_LEFT && !keyRepeatDelay) {
			selectedTheme -= 10;
			if(selectedTheme < 0) {
				selectedTheme = 0;
			}
			keyRepeatDelay = 3;
		} else if(hHeld & KEY_RIGHT && !keyRepeatDelay) {
			selectedTheme += 10;
			if(selectedTheme > (int)themeList.size()) {
				selectedTheme = themeList.size()-1;
			}
			keyRepeatDelay = 3;
		}
		std::string themesText;
		for(int i=(selectedTheme<10) ? 0 : selectedTheme-10;i<(int)themeList.size()&&i<((selectedTheme<10) ? 11 : selectedTheme+1);i++) {
			if(i == selectedTheme) {
				themesText += "> " + themeList[i].name + "\n";
			} else {
				themesText += "  " + themeList[i].name + "\n";
			}
		}
		for(uint i=0;i<((themeList.size()<10) ? 11-themeList.size() : 0);i++) {
			themesText += "\n";
		}
		themesText += "B: Back   A: Choose";
		displayBottomMsg(themesText.c_str());
	}
}
