#include "animation.hpp"
#include "cia.hpp"
#include "download.hpp"
#include "extract.hpp"
#include "fileBrowse.hpp"
#include "formatting.hpp"
#include "gui.hpp"
#include "inifile.h"
#include "keyboard.h"
#include "thread.h"

#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <unistd.h>

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
std::string usernamePasswordCache = "";

extern C3D_RenderTarget* Top;
extern C3D_RenderTarget* Bottom;

extern bool downloadNightlies;
extern bool updateAvailable[];
extern bool updated3dsx;
extern int filesExtracted;
extern std::string extractingFile;

char progressBarMsg[128] = "";
bool showProgressBar = false;
int progressBarType = 0; // 0 = Download | 1 = Extract | 2 = Install

// That are our extract Progressbar variables.
extern u64 extractSize, writeOffset;
// That are our install Progressbar variables.
extern u64 installSize, installOffset;
bool continueNdsScan = true;

curl_off_t downloadTotal = 1; //Dont initialize with 0 to avoid division by zero later
curl_off_t downloadNow = 0;

static FILE *downfile = NULL;
static size_t file_buffer_pos = 0;
static size_t file_toCommit_size = 0;
static char* g_buffers[2] = { NULL };
static u8 g_index = 0;
static Thread fsCommitThread;
static LightEvent readyToCommit;
static LightEvent waitCommit;
static bool killThread = false;
static bool writeError = false;
#define FILE_ALLOC_SIZE 0x60000

static int curlProgress(CURL *hnd,
					curl_off_t dltotal, curl_off_t dlnow,
					curl_off_t ultotal, curl_off_t ulnow)
{
	downloadTotal = dltotal;
	downloadNow = dlnow;

	return 0;
}

bool filecommit() {
	if (!downfile) return false;
	fseek(downfile, 0, SEEK_END);
	u32 byteswritten = fwrite(g_buffers[!g_index], 1, file_toCommit_size, downfile);
	if (byteswritten != file_toCommit_size) return false;
	file_toCommit_size = 0;
	return true;
}

static void commitToFileThreadFunc(void* args) {
	LightEvent_Signal(&waitCommit);
	while (true) {
		LightEvent_Wait(&readyToCommit);
		LightEvent_Clear(&readyToCommit);
		if (killThread) threadExit(0);
		writeError = !filecommit();
		LightEvent_Signal(&waitCommit);
	}
}

static size_t file_handle_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
	(void)userdata;
	const size_t bsz = size * nmemb;
	size_t tofill = 0;
	if (writeError) return 0;
	if (!g_buffers[g_index]) {

		LightEvent_Init(&waitCommit, RESET_STICKY);
		LightEvent_Init(&readyToCommit, RESET_STICKY);

		s32 prio = 0;
		svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
		fsCommitThread = threadCreate(commitToFileThreadFunc, NULL, 0x1000, prio - 1, -2, true);

		g_buffers[0] = (char*)memalign(0x1000, FILE_ALLOC_SIZE);
		g_buffers[1] = (char*)memalign(0x1000, FILE_ALLOC_SIZE);

		if (!fsCommitThread || !g_buffers[0] || !g_buffers[1]) return 0;
	}
	if (file_buffer_pos + bsz >= FILE_ALLOC_SIZE) {
		tofill = FILE_ALLOC_SIZE - file_buffer_pos;
		memcpy(g_buffers[g_index] + file_buffer_pos, ptr, tofill);
		
		LightEvent_Wait(&waitCommit);
		LightEvent_Clear(&waitCommit);
		file_toCommit_size = file_buffer_pos + tofill;
		file_buffer_pos = 0;
		svcFlushProcessDataCache(CUR_PROCESS_HANDLE, g_buffers[g_index], file_toCommit_size);
		g_index = !g_index;
		LightEvent_Signal(&readyToCommit);
	}
	memcpy(g_buffers[g_index] + file_buffer_pos, ptr + tofill, bsz - tofill);
	file_buffer_pos += bsz - tofill;
	return bsz;
}

Result downloadToFile(std::string url, std::string path) {

	Result retcode = 0;
	downloadTotal = 1;
	downloadNow = 0;
	int res;
	CURL *hnd;
	CURLcode cres;

	printf("Downloading from:\n%s\nto:\n%s\n", url.c_str(), path.c_str());
	const char* filepath = path.c_str();

	void *socubuf = memalign(0x1000, 0x100000);
	if (!socubuf) {
		retcode = -1;
		goto exit;
	}
	
	res = socInit((u32*)socubuf, 0x100000);
	if (R_FAILED(res)) {
		retcode = res;
		goto exit;
	}

	makeDirs(strdup(filepath));
	downfile = fopen(filepath, "wb");
	if (!downfile) {
		retcode = -2;
		goto exit;
	}

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, FILE_ALLOC_SIZE);
	curl_easy_setopt(hnd, CURLOPT_URL, url.c_str());
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(hnd, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(hnd, CURLOPT_ACCEPT_ENCODING, "gzip");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_XFERINFOFUNCTION, curlProgress);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, file_handle_data);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(hnd, CURLOPT_STDERR, stdout);

	cres = curl_easy_perform(hnd);
	curl_easy_cleanup(hnd);
	
	if (cres != CURLE_OK) {
		retcode = -cres;
		goto exit;
	}

	LightEvent_Wait(&waitCommit);
	LightEvent_Clear(&waitCommit);

	file_toCommit_size = file_buffer_pos;
	svcFlushProcessDataCache(CUR_PROCESS_HANDLE, g_buffers[g_index], file_toCommit_size);
	g_index = !g_index;
	if (!filecommit()) {
		retcode = -3;
		goto exit;
	}
	fflush(downfile);
	
exit:
	if (fsCommitThread) {
		killThread = true;
		LightEvent_Signal(&readyToCommit);
		threadJoin(fsCommitThread, U64_MAX);
		killThread = false;
		fsCommitThread = NULL;
	}

	socExit();
	
	if (socubuf) {
		free(socubuf);
	}
	if (downfile) {
		fclose(downfile);
		downfile = NULL;
	}
	if (g_buffers[0]) {
		free(g_buffers[0]);
		g_buffers[0] = NULL;
	}
	if (g_buffers[1]) {
		free(g_buffers[1]);
		g_buffers[1] = NULL;
	}
	g_index = 0;
	file_buffer_pos = 0;
	file_toCommit_size = 0;
	writeError = false;
	
	return retcode;
}

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

static Result setupContext(CURL *hnd, const char * url) {
	downloadTotal = 1;
	downloadNow = 0;
	curl_easy_setopt(hnd, CURLOPT_XFERINFOFUNCTION, curlProgress);

	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_URL, url);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, handle_data);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(hnd, CURLOPT_STDERR, stdout);
	if(usernamePasswordCache != "")	curl_easy_setopt(hnd, CURLOPT_USERPWD, usernamePasswordCache.c_str());

	return 0;
}

Result downloadFromRelease(std::string url, std::string asset, std::string path) {
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if(!socubuf) {
		return -1;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if(R_FAILED(ret)) {
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
	if(ret != 0) {
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

	if(cres != CURLE_OK) {
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
	if(parsedAPI["assets"].is_array()) {
		for (auto jsonAsset : parsedAPI["assets"]) {
			if(jsonAsset.is_object() && jsonAsset["name"].is_string() && jsonAsset["browser_download_url"].is_string()) {
				std::string assetName = jsonAsset["name"];
				if(matchPattern(asset, assetName)) {
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

	if(assetUrl.empty())
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
	//return true; // Citra
	u32 wifiStatus;
	bool res = false;

	if(R_SUCCEEDED(ACU_GetWifiStatus(&wifiStatus)) && wifiStatus) {
		res = true;
	}

	return res;
}

void downloadFailed(void) {
	Msg::DisplayMsg("Download failed!");
	for (int i = 0; i < 60*2; i++) {
		gspWaitForVBlank();
	}
}

void notConnectedMsg(void) {
	Msg::DisplayMsg("Please connect to Wi-Fi");
	for (int i = 0; i < 60*2; i++) {
		gspWaitForVBlank();
	}
}

void doneMsg(void) {
	Msg::DisplayMsg("Done!");
	for (int i = 0; i < 60*2; i++) {
		gspWaitForVBlank();
	}
}

std::string getLatestRelease(std::string repo, std::string item, bool retrying) {
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if(!socubuf) {
		return "";
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if(R_FAILED(ret)) {
		free(socubuf);
		return "";
	}

	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/releases/latest";
	std::string apiurl = apiurlStream.str();

	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if(ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return "";
	}

	CURLcode cres = curl_easy_perform(hnd);
	int httpCode;
	curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string

	if(httpCode == 401 || httpCode == 403) {
		bool save = promtUsernamePassword();
		if(save)	saveUsernamePassword();
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		if(usernamePasswordCache == "" || retrying) {
			usernamePasswordCache = "";
			return "API";
		} else {
			return getLatestRelease(repo, item, true);
		}
	}

	if(cres != CURLE_OK) {
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
	if(parsedAPI[item].is_string()) {
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

std::vector<std::string> getRecentCommits(std::string repo, std::string item, bool retrying) {
	std::vector<std::string> emptyVector;
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if(!socubuf) {
		return emptyVector;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if(R_FAILED(ret)) {
		free(socubuf);
		return emptyVector;
	}

	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/commits";
	std::string apiurl = apiurlStream.str();

	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if(ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return emptyVector;
	}

	CURLcode cres = curl_easy_perform(hnd);
	int httpCode;
	curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string

	if(httpCode == 401 || httpCode == 403) {
		bool save = promtUsernamePassword();
		if(save)	saveUsernamePassword();
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		if(usernamePasswordCache == "" || retrying) {
			usernamePasswordCache = "";
			return {"API"};
		} else {
			return getRecentCommits(repo, item, true);
		}
	}

	if(cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return emptyVector;
	}

	std::vector<std::string> jsonItems;
	json parsedAPI = json::parse(result_buf);
	for(uint i=0;i<parsedAPI.size();i++) {
		if(parsedAPI[i][item].is_string()) {
			jsonItems.push_back(parsedAPI[i][item]);
		}
	}

	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;

	return jsonItems;
}

std::vector<std::string> getRecentCommitsArray(std::string repo, std::string array, std::string item, bool retrying) {
	std::vector<std::string> emptyVector;
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	if(!socubuf) {
		return emptyVector;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if(R_FAILED(ret)) {
		free(socubuf);
		return emptyVector;
	}

	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/commits";
	std::string apiurl = apiurlStream.str();

	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if(ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return emptyVector;
	}

	CURLcode cres = curl_easy_perform(hnd);
	int httpCode;
	curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string

	if(httpCode == 401 || httpCode == 403) {
		bool save = promtUsernamePassword();
		if(save)	saveUsernamePassword();
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		if(usernamePasswordCache == "" || retrying) {
			usernamePasswordCache = "";
			return {"API"};
		} else {
			return getRecentCommitsArray(repo, array, item, true);
		}
	}

	if(cres != CURLE_OK) {
		printf("Error in:\ncurl\n");
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return emptyVector;
	}

	std::vector<std::string> jsonItems;
	json parsedAPI = json::parse(result_buf);
	for(uint i=0;i<parsedAPI.size();i++) {
		if(parsedAPI[i][array][item].is_string()) {
			jsonItems.push_back(parsedAPI[i][array][item]);
		}
	}

	socExit();
	free(result_buf);
	free(socubuf);
	result_buf = NULL;
	result_sz = 0;
	result_written = 0;

	return jsonItems;
}

std::vector<ThemeEntry> getThemeList(std::string repo, std::string path, bool retrying) {
	Result ret = 0;
	void *socubuf = memalign(0x1000, 0x100000);
	std::vector<ThemeEntry> emptyVector;
	if(!socubuf) {
		return emptyVector;
	}

	ret = socInit((u32*)socubuf, 0x100000);
	if(R_FAILED(ret)) {
		free(socubuf);
		return emptyVector;
	}

	std::stringstream apiurlStream;
	apiurlStream << "https://api.github.com/repos/" << repo << "/contents/" << path;
	std::string apiurl = apiurlStream.str();

	CURL *hnd = curl_easy_init();
	ret = setupContext(hnd, apiurl.c_str());
	if(ret != 0) {
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		return emptyVector;
	}

	CURLcode cres = curl_easy_perform(hnd);
	int httpCode;
	curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup(hnd);
	char* newbuf = (char*)realloc(result_buf, result_written + 1);
	result_buf = newbuf;
	result_buf[result_written] = 0; //nullbyte to end it as a proper C style string

	if(httpCode == 401 || httpCode == 403) { // Unauthorized || Forbidden
		bool save = promtUsernamePassword();
		if(save)	saveUsernamePassword();
		socExit();
		free(result_buf);
		free(socubuf);
		result_buf = NULL;
		result_sz = 0;
		result_written = 0;
		if(usernamePasswordCache == "" || retrying) {
			usernamePasswordCache = "";
			return {{"API", "API", "API", "API"}};
		} else {
			return getThemeList(repo, path, true);
		}
	}

	if(cres != CURLE_OK) {
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
		if(parsedAPI[i]["name"].is_string()) {
			themeEntry.name = parsedAPI[i]["name"];
		}
		if(parsedAPI[i]["download_url"].is_string()) {
			themeEntry.downloadUrl = parsedAPI[i]["download_url"];
		}
		if(parsedAPI[i]["path"].is_string()) {
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
			Msg::DisplayMsg(("Downloading: "+themeContents[i].name).c_str());
			downloadToFile(themeContents[i].downloadUrl, themeContents[i].sdPath);
		} else {
			Msg::DisplayMsg(("Downloading: "+themeContents[i].name).c_str());
			mkdir((themeContents[i].sdPath).c_str(), 0777);
			downloadTheme(themeContents[i].path);
		}
	}
}

bool showReleaseInfo(std::string repo, bool showExitText) {
	Msg::DisplayMsg("Loading release notes...");
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

		if(hDown & KEY_A) {
			return true;
		} else if(hDown & KEY_B || hDown & KEY_Y || hDown & KEY_TOUCH) {
			return false;
		} else if(hHeld & KEY_UP) {
			if(textPosition > 0) {
				textPosition--;
				redrawText = true;
			}
		} else if(hHeld & KEY_DOWN) {
			if(textPosition < (int)(_topText.size() - 10)) {
				textPosition++;
				redrawText = true;
			}
		}
	}
}

std::string chooseCommit(std::string repo, std::string title, bool showExitText) {
	Msg::DisplayMsg("Loading commits...");
	std::vector<std::string> jsonShasTemp = getRecentCommits(repo, "sha");
	std::vector<std::string> jsonBodyTemp = ((jsonShasTemp[0] == "API") ? jsonShasTemp : getRecentCommitsArray(repo, "commit", "message"));
	std::vector<std::string> jsonShas;
	std::vector<std::string> jsonBody;
	gspWaitForVBlank();

	jsonBody.push_back("Latest");
	jsonShas.push_back("master");

	if(jsonShasTemp[0] != "API"){
		for(uint i=0;i<jsonShasTemp.size();i++) {
			if(jsonBodyTemp[i].substr(0, title.size()) == title) {
				jsonBody.push_back(jsonBodyTemp[i]);
				jsonShas.push_back(jsonShasTemp[i]);
			}
		}
	}

	int selectedCommit = 0;
	int keyRepeatDelay = 0;
	showCommitList:
	while(1) {
		gspWaitForVBlank();
		hidScanInput();
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();
		if(keyRepeatDelay)	keyRepeatDelay--;
		if(hDown & KEY_A) {
			break;
		} else if(hDown & KEY_B) {
			return "";
		} else if(hHeld & KEY_UP && !keyRepeatDelay) {
			if(selectedCommit > 0) {
				selectedCommit--;
				keyRepeatDelay = 3;
			}
		} else if(hHeld & KEY_DOWN && !keyRepeatDelay) {
			if(selectedCommit < (int)jsonShas.size()-1) {
				selectedCommit++;
				keyRepeatDelay = 3;
			}
		} else if(hHeld & KEY_LEFT && !keyRepeatDelay) {
			selectedCommit -= 10;
			if(selectedCommit < 0) {
				selectedCommit = 0;
			}
			keyRepeatDelay = 3;
		} else if(hHeld & KEY_RIGHT && !keyRepeatDelay) {
			selectedCommit += 10;
			if(selectedCommit > (int)jsonBody.size()) {
				selectedCommit = jsonBody.size()-1;
			}
			keyRepeatDelay = 3;
		}
		std::string commitsText = "Choose a commit for commit notes:\n";
		for(int i=(selectedCommit<9) ? 0 : selectedCommit-9;i<(int)jsonBody.size()&&i<((selectedCommit<9) ? 10 : selectedCommit+1);i++) {
			if(i == selectedCommit) {
				commitsText += "> " + jsonBody[i] + "\n";
			} else {
				commitsText += "  " + jsonBody[i] + "\n";
			}
		}
		for(uint i=0;i<((jsonBody.size()<9) ? 10-jsonBody.size() : 0);i++) {
			commitsText += "\n";
		}
		commitsText += "B: Back   A: Info";
		Msg::DisplayMsg(commitsText);
	}

	setMessageText(jsonBody[selectedCommit]);
	jsonName = jsonShas[selectedCommit].substr(0,7);
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

		if(hDown & KEY_A) {
			return jsonShas[selectedCommit].substr(0,7);
		} else if(hDown & KEY_B || hDown & KEY_Y || hDown & KEY_TOUCH) {
			goto showCommitList;
		} else if(hHeld & KEY_UP) {
			if(textPosition > 0) {
				textPosition--;
				redrawText = true;
			}
		} else if(hHeld & KEY_DOWN) {
			if(textPosition < (int)(_topText.size() - 10)) {
				textPosition++;
				redrawText = true;
			}
		}
	}
}

void setMessageText(const std::string &text) {
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
	for(auto word : words) {
		int width = Gui::GetStringWidth(0.5f, (temp + " " + word).c_str());
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

void drawMessageText(int position, bool showExitText) {
	Gui::clearTextBufs();
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(Bottom, TRANSPARENT);
	Gui::ScreenDraw(Bottom);
	GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
	Gui::DrawString(18, 24, .7, BLACK, jsonName.c_str());
	for (int i = 0; i < (int)_topText.size() && i < (showExitText ? 9 : 10); i++) {
		Gui::DrawString(24, ((i * 16) + 48), 0.5f, BLACK, _topText[i+position].c_str());
	}
	if(showExitText)
		Gui::DrawString(24, 200, 0.5f, BLACK, "B: Cancel   A: Update");
	C3D_FrameEnd(0);
}


void displayProgressBar() {
	char str[256];
	while(showProgressBar) {
		if (downloadTotal < 1.0f) {
			downloadTotal = 1.0f;
		}
		if (downloadTotal < downloadNow) {
			downloadTotal = downloadNow;
		}

		// Downloading.
		if (progressBarType == 0){
			snprintf(str, sizeof(str), "%s / %s (%.2f%%)",
					formatBytes(downloadNow).c_str(),
					formatBytes(downloadTotal).c_str(),
					((float)downloadNow/(float)downloadTotal) * 100.0f);
					// Extracting.
		} else if (progressBarType == 1) {
			snprintf(str, sizeof(str), "%s / %s (%.2f%%)",
					formatBytes(writeOffset).c_str(),
					formatBytes(extractSize).c_str(),
					((float)writeOffset/(float)extractSize) * 100.0f);
			// Installing.
		} else if (progressBarType == 2){
			snprintf(str, sizeof(str), "%s / %s (%.2f%%)",
					formatBytes(installOffset).c_str(),
					formatBytes(installSize).c_str(),
					((float)installOffset/(float)installSize) * 100.0f);
		};

		Gui::clearTextBufs();
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(Top, BLACK);
		C2D_TargetClear(TopRight, BLACK);
		C2D_TargetClear(Bottom, BLACK);
		GFX::DrawTop(false);
		Gui::ScreenDraw(Bottom);
		GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
		Gui::DrawStringCentered(0, 40, 0.65f, BLACK, progressBarMsg, 400);
		// Only display this by downloading.
		if (progressBarType == 0) {
			Gui::DrawStringCentered(0, 100, 0.6f, BLACK, str, 300);
			// Outline of progressbar.
			Gui::Draw_Rect(60, 169, 202, 30, BLACK);
			Animation::DrawProgressBar(downloadNow, downloadTotal);
		}
		// Only Display this by extracting.
		if (progressBarType == 1) {
			// Text.
			Gui::DrawStringCentered(0, 100, 0.4f, BLACK, str, 300);
			Gui::DrawStringCentered(0, 150, 0.4f, BLACK, std::to_string(filesExtracted) + " " + (filesExtracted == 1 ? ("file extracted.") :("files extracted.")), 300);
			Gui::DrawStringCentered(0, 120, 0.4f, BLACK, "Currently extracting:\n" + extractingFile, 300);
			// Outline of progressbar.
			Gui::Draw_Rect(60, 169, 202, 30, BLACK);
			// Progressbar.
			Animation::DrawProgressBar((float)writeOffset, (float)extractSize);
		}
		// Only display this by installing.
		if (progressBarType == 2) {
			Gui::DrawStringCentered(0, 100, 0.4f, BLACK, str, 300);
			// Outline of progressbar.
			Gui::Draw_Rect(60, 169, 202, 30, BLACK);
			Animation::DrawProgressBar((float)installOffset, (float)installSize);
		}
		C3D_FrameEnd(0);
		gspWaitForVBlank();
	}
}

bool promtUsernamePassword(void) {
	Msg::DisplayMsg("The GitHub API rate limit has been\n"
					 "exceeded for your IP, you can regain\n"
					 "access by signing in to a GitHub account\n"
					 "or waiting for a bit\n"
					 "(or press B but some things won't work)\n\n\n\n\n\n\n\n"
					 "B: Cancel   A: Authenticate");

	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	Gui::ScreenDraw(Bottom);
	Gui::DrawString(20, 100, 0.45f, BLACK, "Username:");
	Gui::Draw_Rect(100, 100, 100, 14, GRAY);
	Gui::DrawString(20, 120, 0.45f, BLACK, "Password:");
	Gui::Draw_Rect(100, 120, 100, 14, GRAY);

	Gui::Draw_Rect(100, 140, 14, 14, GRAY);
	Gui::DrawString(120, 140, 0.45f, BLACK, "Save login?");
	C3D_FrameEnd(0);

	bool save = false;
	int hDown;
	std::string username, password;
	while(1) {
		do {
			gspWaitForVBlank();
			hidScanInput();
			hDown = hidKeysDown();
		} while(!hDown);

		if(hDown & KEY_A) {
			usernamePasswordCache = username + ":" + password;
			return save;
		} else if(hDown & KEY_B) {
			usernamePasswordCache = "";
			return save;
		} else if(hDown & KEY_TOUCH) {
			touchPosition touch;
			touchRead(&touch);
			if(touch.px >= 100 && touch.px <= 200 && touch.py >= 100 && touch.py <= 114) {
				username = keyboardInput("Username");
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
				Gui::ScreenDraw(Bottom);
				Gui::Draw_Rect(100, 100, 100, 14, GRAY);
				Gui::DrawString(100, 100, 0.45f, BLACK, username.c_str());
				C3D_FrameEnd(0);
			} else if(touch.px >= 100 && touch.px <= 200 && touch.py >= 120 && touch.py <= 134) {
				password = keyboardInput("Password");
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
				Gui::ScreenDraw(Bottom);
				Gui::Draw_Rect(100, 120, 100, 14, GRAY);
				Gui::DrawString(100, 120, 0.45f, BLACK, password.c_str());
				C3D_FrameEnd(0);
			} else if(touch.px >= 100 && touch.px <= 114 && touch.py >= 140 && touch.py <= 154) {
				save = !save;
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
				Gui::ScreenDraw(Bottom);
				Gui::Draw_Rect(102, 142, 10, 10, save ? GREEN : GRAY);
				C3D_FrameEnd(0);
			}
		}
	}
}

void loadUsernamePassword(void) {
	std::ifstream in("sdmc:/_nds/TWiLightMenu/extras/updater/usernamePassword");
	if(in.good())	in >> usernamePasswordCache;
	in.close();
}

void saveUsernamePassword() {
	std::ofstream out("sdmc:/_nds/TWiLightMenu/extras/updater/usernamePassword");
	if(out.good())	out << usernamePasswordCache;
	out.close();
}

std::string latestMenuRelease(void) {
	if(latestMenuReleaseCache == "") {
		std::string apiInfo = getLatestRelease("DS-Homebrew/TWiLightMenu", "tag_name");
		if(apiInfo != "API") {
			latestMenuReleaseCache = apiInfo;
		}
	}
	return latestMenuReleaseCache;
}

std::string latestMenuNightly(void) {
	if(latestMenuNightlyCache == "") {
		std::string apiInfo = getRecentCommits("DS-Homebrew/TWiLightMenu", "sha")[0];
		if(apiInfo != "API") {
			latestMenuNightlyCache = apiInfo.substr(0,7);
		}
	}
	return latestMenuNightlyCache;
}

std::string latestBootstrapRelease(void) {
	if(latestBootstrapReleaseCache == "") {
		std::string apiInfo = getLatestRelease("ahezard/nds-bootstrap", "tag_name");
		if(apiInfo != "API") {
			latestBootstrapReleaseCache = apiInfo;
		}
	}
	return latestBootstrapReleaseCache;
}

std::string latestBootstrapNightly(void) {
	if(latestBootstrapNightlyCache == "") {
		std::string apiInfo = getRecentCommits("ahezard/nds-bootstrap", "sha")[0];
		if(apiInfo != "API") {
			latestBootstrapNightlyCache = apiInfo.substr(0,7);
		}
	}
	return latestBootstrapNightlyCache;
}

std::string latestUpdaterRelease(void) {
	if(latestUpdaterReleaseCache == "") {
		std::string apiInfo = getLatestRelease("RocketRobz/TWiLightMenu-Updater", "tag_name");
		if(apiInfo != "API") {
			latestUpdaterReleaseCache = apiInfo;
		}
	}
	return latestUpdaterReleaseCache;
}

std::string latestUpdaterNightly(void) {
	if(latestUpdaterNightlyCache == "") {
		std::string apiInfo = getRecentCommits("RocketRobz/TWiLightMenu-Updater", "sha")[0];
		if(apiInfo != "API") {
			latestUpdaterNightlyCache = apiInfo.substr(0,7);
		}
	}
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

	if(menuChannel == "release")
		updateAvailable[0] = menuVersion != latestMenuRelease();
	else if(menuChannel == "nightly")
		updateAvailable[1] = menuVersion != latestMenuNightly();
	else
		updateAvailable[0] = updateAvailable[1] = true;

	updateAvailable[2] = bootstrapRelease != latestBootstrapRelease();
	updateAvailable[3] = boostrapNightly != latestBootstrapNightly();

	if(updaterChannel == "release")
		updateAvailable[4] = updaterVersion != latestUpdaterRelease();
	else if(updaterChannel == "nightly")
		updateAvailable[5] = updaterVersion != latestUpdaterNightly();
	else
		updateAvailable[4] = updateAvailable[5] = true;
}

void updateBootstrap(std::string commit) {
	if(commit != "") {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading nds-bootstrap...\n(Nightly)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadToFile("https://github.com/TWLBot/Builds/blob/"+commit+"/nds-bootstrap.7z?raw=true", "/nds-bootstrap-nightly.7z") != 0) {
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
		Msg::DisplayMsg("Downloading nds-bootstrap...\n(Release)");
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading nds-bootstrap...\n(Release)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadFromRelease("https://github.com/ahezard/nds-bootstrap", "nds-bootstrap\\.zip", "/nds-bootstrap-release.zip") != 0) {
			showProgressBar = false;
			downloadFailed();
			return;
		}

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting nds-bootstrap...\n(Release)");
		filesExtracted = 0;
		progressBarType = 1;
		extractArchive("/nds-bootstrap-release.zip", "", "/_nds/");
		showProgressBar = false;

		deleteFile("sdmc:/nds-bootstrap-release.zip");

		setInstalledVersion("NDS-BOOTSTRAP-RELEASE", latestBootstrapRelease());
		saveUpdateData();
		updateAvailable[2] = false;
	}
	doneMsg();
}

void updateTWiLight(std::string commit) {
	if(commit != "") {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading TWiLight Menu++...\n(Nightly)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadToFile("https://github.com/TWLBot/Builds/blob/"+commit+"/TWiLightMenu.7z?raw=true", "/TWiLightMenu-nightly.7z") != 0) {
			showProgressBar = false;
			downloadFailed();
			return;
		}

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Extracting TWiLight Menu++...\n(Nightly)");
		filesExtracted = 0;
		progressBarType = 1;
		extractArchive("/TWiLightMenu-nightly.7z", "_nds/", "/_nds/");
		extractArchive("/TWiLightMenu-nightly.7z", "3DS - CFW users/", "/");
		extractArchive("/TWiLightMenu-nightly.7z", "DSi&3DS - SD card users/", "/");
		

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Installing TWiLight Menu++ CIA...\n(Nightly)");
		progressBarType = 2;
		installCia("/TWiLight Menu.cia", false);
		installCia("/TWiLight Menu - Game booter.cia", false);
		showProgressBar = false;

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
		if(downloadFromRelease("https://github.com/DS-Homebrew/TWiLightMenu", "TWiLightMenu\\.7z", "/TWiLightMenu-release.7z") != 0) {
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

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Installing TWiLight Menu++ CIA...\n(Release)");
		progressBarType = 2;
		installCia("/TWiLight Menu.cia", false);
		installCia("/TWiLight Menu - Game booter.cia", false);
		showProgressBar = false;
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

void updateSelf(std::string commit) {
	if(commit != "" && (access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) != 0)) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading the Updater CIA...\n(Nightly)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadToFile("https://github.com/TWLBot/Builds/blob/"+commit+"/TWiLightMenu%20Updater/TWiLight_Menu++_Updater.cia?raw=true", "/TWiLightMenu-Updater-nightly.cia") != 0) {
			downloadFailed();
			showProgressBar = false;
			return;
		}

		// Before the cia install because rebooting the app might've been causing this file to get cleared
		setInstalledChannel("TWILIGHTMENU-UPDATER", "nightly");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterNightly());
		saveUpdateData();
		updateAvailable[5] = false;

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Installing the Updater CIA... (Nightly)");
		progressBarType = 2;
		installCia("/TWiLightMenu-Updater-nightly.cia", true);
		showProgressBar = false;
		deleteFile("sdmc:/TWiLightMenu-Updater-nightly.cia");
	} else if(commit == "" && (access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) != 0)) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading the Updater CIA...\n(Release)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadFromRelease("https://github.com/RocketRobz/TWiLightMenu-Updater", "TWiLightMenu-Updater\\.cia", "/TWiLightMenu-Updater-release.cia") != 0) {
			downloadFailed();
			showProgressBar = false;
			return;
		}

		// Before the cia install because rebooting the app might've been causing this file to get cleared
		setInstalledChannel("TWILIGHTMENU-UPDATER", "release");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterRelease());
		saveUpdateData();
		updateAvailable[4] = false;

		snprintf(progressBarMsg, sizeof(progressBarMsg), "Installing the Updater CIA...\n(Release)");
		progressBarType = 2;
		installCia("/TWiLightMenu-Updater-release.cia", true);
		showProgressBar = false;

		deleteFile("sdmc:/TWiLightMenu-Updater-release.cia");
	} else if(commit != "" && (access("sdmc:/3ds/TWiLightMenu-Updater.3dsx", F_OK) == 0)) {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading the Updater 3dsx...\n(Nightly)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadToFile("https://github.com/TWLBot/Builds/blob/"+commit+"/TWiLightMenu%20Updater/TWiLight_Menu++_Updater.3dsx?raw=true", "/3ds/TWiLightMenu-Updater.3dsx") != 0) {
			downloadFailed();
			return;
		}
		showProgressBar = false;

		setInstalledChannel("TWILIGHTMENU-UPDATER", "nightly");
		setInstalledVersion("TWILIGHTMENU-UPDATER", latestUpdaterNightly());
		saveUpdateData();
		updateAvailable[5] = false;
	} else {
		snprintf(progressBarMsg, sizeof(progressBarMsg), "Downloading the Updater 3dsx...\n(Release)");
		showProgressBar = true;
		progressBarType = 0;
		createThread((ThreadFunc)displayProgressBar);
		if(downloadFromRelease("https://github.com/RocketRobz/TWiLightMenu-Updater", "TWiLightMenu-Updater\\.3dsx", "/3ds/TWiLightMenu-Updater.3dsx") != 0) {
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
	if(downloadToFile("https://github.com/TWLBot/Builds/raw/master/usrcheat.dat.7z", "/usrcheat.dat.7z") != 0) {
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
			// if(region == CFG_REGION_USA) {
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

void downloadExtras(void) {
	std::string extrasOptions[] = {"Boxart", "Themes"};
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
		std::string extrasText = "What would you like to download?\n";
		for(int i=0;i<2;i++) {
			if(i == selectedOption) {
				extrasText += "> " + extrasOptions[i] + "\n";
			} else {
				extrasText += "  " + extrasOptions[i] + "\n";
			}
		}
		extrasText += "\n\n\n\n\n\n\n\n";
		extrasText += "B: Back   A: Choose";
		Msg::DisplayMsg(extrasText.c_str());
	}
}

void downloadBoxart(void) {

	vector<DirEntry> dirContents;
	std::string scanDir;

	Msg::DisplayMsg("Would you like to choose a directory, or scan\nthe full card?\n\n\n\n\n\n\n\n\n\nB: Cancel   A: Choose Directory   X: Full SD");

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
					Msg::DisplayMsg(dirs.c_str());
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

	Msg::DisplayMsg("Scanning SD card for DS roms...\n\n(Press B to cancel)");

	chdir(scanDir.c_str());
	continueNdsScan = true;
	createThread((ThreadFunc)scanToCancelBoxArt);
	findNdsFiles(dirContents);
	continueNdsScan = false;

	mkdir("sdmc:/_nds/TWiLightMenu/boxart/temp", 0777);
	for(int i=0;i<(int)dirContents.size();i++) {
		char path[256];
		snprintf(path, sizeof(path), "sdmc:/_nds/TWiLightMenu/boxart/%s.png", dirContents[i].tid);
		if(access(path, F_OK) != 0) {
			char downloadMessage[50];
			snprintf(downloadMessage, sizeof(downloadMessage), "Downloading \"%s.png\"...\n", dirContents[i].tid);
			Msg::DisplayMsg(downloadMessage);

			const char *ba_region = getBoxartRegion(dirContents[i].tid[3]);

			char boxartUrl[256];
			snprintf(boxartUrl, sizeof(boxartUrl), "https://art.gametdb.com/ds/coverS/%s/%s.png", ba_region, dirContents[i].tid);
			char boxartPath[256];
			snprintf(boxartPath, sizeof(boxartPath), "/_nds/TWiLightMenu/boxart/temp/%s.png", dirContents[i].tid);

			downloadToFile(boxartUrl, boxartPath);
		}
	}

	chdir("sdmc:/_nds/TWiLightMenu/boxart/temp/");
	getDirectoryContents(dirContents);

	Msg::DisplayMsg("Cleaning up...");
	for(int i=0;i<(int)dirContents.size();i++) {
		if(dirContents[i].size == 0) {
			char path[256];
			snprintf(path, sizeof(path), "sdmc:/_nds/TWiLightMenu/boxart/temp/%s", dirContents[i].name.c_str());
			deleteFile(path);
		} else {
			char tempPath[256];
			snprintf(tempPath, sizeof(tempPath), "sdmc:/_nds/TWiLightMenu/boxart/temp/%s", dirContents[i].name.c_str());
			char path[256];
			snprintf(path, sizeof(path), "sdmc:/_nds/TWiLightMenu/boxart/%s", dirContents[i].name.c_str());
			deleteFile(path);
			rename(tempPath, path);
		}
		rmdir("sdmc:/_nds/TWiLightMenu/boxart/temp");
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
		Msg::DisplayMsg(themesText.c_str());
	}

	Msg::DisplayMsg("Getting theme list...");

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
			Msg::DisplayMsg(("Downloading: "+themeList[selectedTheme].name).c_str());
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
		Msg::DisplayMsg(themesText.c_str());
	}
}
