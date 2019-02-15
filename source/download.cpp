#include "download.hpp"
#include <sys/stat.h>

#include "extract.hpp"
#include "inifile.h"

extern "C" {
	#include "cia.h"
}

#define  USER_AGENT   APP_TITLE "-" VERSION_STRING

static char* result_buf = NULL;
static size_t result_sz = 0;
static size_t result_written = 0;

extern void displayBottomMsg(const char* text);

extern bool downloadNightlies;
extern bool updateAvailable[];

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

void updateBootstrap(bool nightly) {
	if(nightly) {
		displayBottomMsg("Downloading nds-bootstrap...\n"
						"(Nightly)");
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/nds-bootstrap.7z?raw=true", "/nds-bootstrap-nightly.7z") != 0) {
			downloadFailed();
			return;
		}

		displayBottomMsg("Extracting nds-bootstrap...\n"
						"(Nightly)");
		extractArchive("/nds-bootstrap-nightly.7z", "nds-bootstrap/", "/_nds/");

		deleteFile("sdmc:/nds-bootstrap-nightly.7z");

		std::string latestNightly = getLatestCommit("ahezard/nds-bootstrap", "sha").substr(0,7);
		CIniFile ini("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		ini.SetString("NDS-BOOTSTRAP", "NIGHTLY", latestNightly);
		ini.SaveIniFile("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		updateAvailable[3] = false;
	} else {	
		displayBottomMsg("Downloading nds-bootstrap...\n"
						"(Release)");
		if (downloadFromRelease("https://github.com/ahezard/nds-bootstrap", "nds-bootstrap\\.zip", "/nds-bootstrap-release.zip") != 0) {
			downloadFailed();
			return;
		}

		displayBottomMsg("Extracting nds-bootstrap...\n"
						"(Release)");
		extractArchive("/nds-bootstrap-release.zip", "/", "/_nds/");

		deleteFile("sdmc:/nds-bootstrap-release.zip");

		std::string latestVersion = getLatestRelease("ahezard/nds-bootstrap", "tag_name");
		CIniFile ini("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		ini.SetString("NDS-BOOTSTRAP", "RELEASE", latestVersion);
		ini.SaveIniFile("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		updateAvailable[2] = false;
	}
	doneMsg();
}

void updateTWiLight(bool nightly) {
	if(nightly) {
		displayBottomMsg("Downloading TWiLight Menu++...\n"
						"(Nightly)");
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/TWiLightMenu.7z?raw=true", "/TWiLightMenu-nightly.7z") != 0) {
			downloadFailed();
			return;
		}

		displayBottomMsg("Extracting TWiLight Menu++...\n"
						"(Nightly)\n\nThis may take a while.");
		extractArchive("/TWiLightMenu-nightly.7z", "TWiLightMenu/_nds/", "/_nds/");
		extractArchive("/TWiLightMenu-nightly.7z", "3DS - CFW users/", "/cia/");

		displayBottomMsg("Installing TWiLight Menu++ CIA...\n"
						"(Nightly)");
		// installCia("/cia/TWiLight Menu.cia");
		installCia("/cia/TWiLight Menu - Game booter.cia");

		deleteFile("sdmc:/TWiLightMenu-nightly.7z");

		std::string latestNightly = getLatestCommit("RocketRobz/TWiLightMenu", "sha").substr(0,7);
		std::string latestVersion = getLatestRelease("RocketRobz/TWiLightMenu", "tag_name");
		CIniFile ini("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		ini.SetString("TWILIGHTMENU", "NIGHTLY", latestNightly);
		ini.SetString("TWILIGHTMENU", "RELEASE", latestVersion);
		ini.SaveIniFile("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		updateAvailable[0] = false;
		updateAvailable[1] = false;
	} else {
		displayBottomMsg("Downloading TWiLight Menu++...\n"
						"(Release)");
		if (downloadFromRelease("https://github.com/RocketRobz/TWiLightMenu", "TWiLightMenu\\.7z", "/TWiLightMenu-release.7z") != 0) {
			downloadFailed();
			return;
		}

		displayBottomMsg("Extracting TWiLight Menu++...\n"
						"(Release)\n\nThis may take a while.");
		extractArchive("/TWiLightMenu-release.7z", "_nds/", "/_nds/");
		extractArchive("/TWiLightMenu-release.7z", "3DS - CFW users/", "/cia/");
		extractArchive("/TWiLightMenu-release.7z", "DSi&3DS - SD card users/", "/");

		displayBottomMsg("Installing TWiLight Menu++ CIA...\n"
						"(Release)");
		installCia("/cia/TWiLight Menu.cia");
		installCia("/cia/TWiLight Menu - Game booter.cia");

		deleteFile("sdmc:/TWiLightMenu-release.7z");

		std::string latestVersion = getLatestRelease("RocketRobz/TWiLightMenu", "tag_name");
		CIniFile ini("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		ini.SetString("TWILIGHTMENU", "RELEASE", latestVersion);
		ini.SaveIniFile("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
		updateAvailable[0] = false;
	}
	doneMsg();
}

void updateSelf(bool nightly) {
	if(nightly) {
		displayBottomMsg("Downloading TWiLight Menu++ Updater...\n"
						"(Nightly)");
		if (downloadToFile("https://github.com/TWLBot/Builds/blob/master/TWiLightMenu%20Updater/TWiLight_Menu++_Updater.cia?raw=true", "/TWiLightMenu-Updater-nightly.cia") != 0) {
			downloadFailed();
			return;
		}

		displayBottomMsg("Installing TWiLight Menu++ Updater CIA...\n"
						"(Nightly)");
		installCia("/TWiLightMenu-Updater-nightly.cia");

		deleteFile("sdmc:/TWiLightMenu-Updater-nightly.cia");
	} else {
		displayBottomMsg("Downloading TWiLight Menu++ Updater...\n"
						"(Release)");
		if (downloadFromRelease("https://github.com/RocketRobz/TWiLightMenu-Updater", "TWiLightMenu-Updater\\.cia", "/TWiLightMenu-Updater-nightly.cia") != 0) {
			downloadFailed();
			return;
		}

		displayBottomMsg("Installing TWiLight Menu++ Updater CIA...\n"
						"(Release)");
		installCia("/TWiLightMenu-Updater-nightly.cia");

		deleteFile("sdmc:/TWiLightMenu-Updater-nightly.cia");
	}
	doneMsg();
}

void updateCheats(void) {
	displayBottomMsg("Downloading DSJ's cheat database v2.1.0...\n");	// This needs to be manually changed when the usrcheat.dat in TWLBot get's updated
	if (downloadToFile("https://github.com/TWLBot/Builds/raw/master/usrcheat.dat.7z", "/usrcheat.dat.7z") != 0) {
		downloadFailed();
		return;
	}

	displayBottomMsg("Extracting DSJ's cheat database v2.1.0...\n"
					"\nThis may take a while.");
	extractArchive("/usrcheat.dat.7z", "usrcheat.dat", "/_nds/TWiLightMenu/extras/usrcheat.dat");

	deleteFile("sdmc:/usrcheat.dat.7z");

	doneMsg();
}
