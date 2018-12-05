#include "download.h"
#include "inifile.h"
#include "settings.h"

#include <cstdio>
#include <malloc.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>
#include <sys/stat.h>
using std::string;
using std::unordered_set;
using std::vector;

#include <3ds.h>
#include "pp2d/pp2d.h"
#include "graphic.h"

#include "json/json.h"

const char* JSON_URL = "https://github.com/RocketRobz/TWiLightMenu-update/raw/master/update.json";
//const char* JSON_NIGHTLIES_URL = "https://github.com/RocketRobz/TWiLightMenu-update/raw/master/updatenightlies.json";
bool updateGBARUNNER_2 = false;

std::string gbarunner2_url;
std::string release_BS_ver;
std::string nightly_BS_ver;
std::string release_BS_url;
std::string nightly_BS_url;
std::string release_hbBS_url;
std::string unofficial_hbBS_url;

std::string nightly_url = "";
std::string nightly_commit = "";
std::string nightly_zip = "";

static char download_textOnScreen[256];

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

/**
 * Download a file.
 * @param url URL of the file.
 * @param file Local filename.
 * @param mediaType How the file should be handled.
 * @return 0 on success; non-zero on error.
 */
int downloadFile(const char* url, const char* file, MediaType mediaType) {
	if (!checkWifiStatus())
		return -1;

	fsInit();
	httpcInit(0x1000);
	httpcContext context;
	u32 statuscode = 0;
	HTTPC_RequestMethod useMethod = HTTPC_METHOD_GET;

	do {
		if (statuscode >= 301 && statuscode <= 308) {
			char newurl[4096];
			httpcGetResponseHeader(&context, (char*)"Location", &newurl[0], 4096);
			url = &newurl[0];

			httpcCloseContext(&context);
		}

		Result ret = httpcOpenContext(&context, useMethod, (char*)url, 0);
		httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);

		if (ret==0) {
			if(R_SUCCEEDED(httpcBeginRequest(&context))){
				u32 contentsize=0;
				if(R_FAILED(httpcGetResponseStatusCode(&context, &statuscode))){
					httpcCloseContext(&context);
					httpcExit();
					fsExit();
					return -1;
				}
				if (statuscode == 200) {
					u32 readSize = 0;
					long int bytesWritten = 0;

					Handle fileHandle;
					FS_Path filePath=fsMakePath(PATH_ASCII, file);
					FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), filePath, FS_OPEN_CREATE | FS_OPEN_WRITE, 0x00000000);

					if(R_FAILED(httpcGetDownloadSizeState(&context, NULL, &contentsize))){
						httpcCloseContext(&context);
						httpcExit();
						fsExit();
						return -1;
					}
					u8* buf = (u8*)malloc(contentsize);
					memset(buf, 0, contentsize);

					do {
						if(R_FAILED(ret = httpcDownloadData(&context, buf, contentsize, &readSize))){
							// In case there is an error
							free(buf);
							httpcCloseContext(&context);
							httpcExit();
							fsExit();
							return -1;
						}
						FSFILE_Write(fileHandle, NULL, bytesWritten, buf, readSize, 0x10001);
						bytesWritten += readSize;
					} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);

					FSFILE_Close(fileHandle);
					svcCloseHandle(fileHandle);

					if (mediaType != MEDIA_SD_FILE) {
						// This is a CIA, so we should install it.
						amInit();
						Handle handle;
						// FIXME: Should check the return values.
						switch (mediaType) {
							case MEDIA_SD_CIA:
								AM_QueryAvailableExternalTitleDatabase(NULL);
								AM_StartCiaInstall(MEDIATYPE_SD, &handle);
								break;
							case MEDIA_NAND_CIA:
								AM_StartCiaInstall(MEDIATYPE_NAND, &handle);
								break;
							default:
								break;
						}
						FSFILE_Write(handle, NULL, 0, buf, contentsize, 0);
						AM_FinishCiaInstall(handle);
						amExit();
					}

					free(buf);
				}  else if ( ((statuscode >= 400) && (statuscode <= 451)) || ((statuscode >= 500) && (statuscode <= 512)) ) {
					// 4XX client error.
					// 5XX server error.
					httpcCloseContext(&context);
					char errorcode_s[4];
					snprintf(errorcode_s, sizeof(errorcode_s), "%lu", statuscode);
					httpcExit();
					fsExit();
					return -1;
				}
			}else{
				// There was an error begining the request
				httpcCloseContext(&context);
				httpcExit();
				fsExit();
				return -1;
			}
		}else{
			// There was a problem opening HTTP context
			httpcCloseContext(&context);
			httpcExit();
			fsExit();
			return -1;
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));
	httpcCloseContext(&context);

	httpcExit();
	fsExit();
	return 0;
}

Result http_read_internal(httpcContext* context, u32* bytesRead, void* buffer, u32 size) {
    if(context == NULL || buffer == NULL) {
        return -1;
    }

    Result res = httpcDownloadData(context, (u8*) buffer, size, bytesRead);
    return res != (int) HTTPC_RESULTCODE_DOWNLOADPENDING ? res : 0;
}

std::vector<std::string> internal_json_reader(json_value* json, json_value* val, vector<std::string> strNames) {
	
	std::vector<std::string> strvalues;
	
	for (int i = 0; i < (int) strNames.size(); i++) {
		for(int j = 0; j < (int) json->u.object.length; j++) {
			char* name = val->u.object.values[j].name;
			int nameLen = val->u.object.values[j].name_length;
			json_value* subVal = val->u.object.values[j].value;	

			if(subVal->type == json_string) {
				if(strncmp(name, strNames[i].c_str(), nameLen) == 0) {
					strvalues.push_back(std::string(subVal->u.string.ptr));
					}
			}
		}
	}
	return strvalues;
}

/**
 * Check for missing files, and download them.
 */
void DownloadMissingFiles(void) {
	u32 responseCode = 0;
	httpcContext context;	
	
	httpcInit(0);
	if(R_FAILED(httpcOpenContext(&context, HTTPC_METHOD_GET, JSON_URL, 0))) {
		return;
	}
	if(R_FAILED(httpcAddRequestHeaderField(&context, "User-Agent", "TWiLightMenu"))) {
		return;
	}
	if(R_FAILED(httpcSetSSLOpt(&context, SSLCOPT_DisableVerify))) {
		return;
	}
	if(R_FAILED(httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED))) {
		return;
	}
	if(R_FAILED(httpcBeginRequest(&context))) {
		return;
	}
	if(R_FAILED(httpcGetResponseStatusCode(&context, &responseCode))) {
		return;
	}
    if (responseCode != 200) {
		return;
	}
	
	u32 size = 0;
	httpcGetDownloadSizeState(&context, NULL, &size);	
	char* jsonText = (char*) calloc(sizeof(char), size);
	if(jsonText != NULL) {
		u32 bytesRead = 0;
		http_read_internal(&context, &bytesRead, (u8*) jsonText, size);
		json_value* json = json_parse(jsonText, size);

		if(json != NULL) {
			if(json->type == json_object) { // {} are objects, [] are arrays				
				json_value* val;
				json_value* val2;
				vector<std::string> strNames;
				vector<std::string> strValues;
				//val = json->u.object.values[1].value;
				//val2 = val->u.object.values[0].value;
				
				// Search in nds-bootstrap object

				val = json->u.object.values[4].value;
				val2 = val->u.object.values[0].value;
				strNames.push_back("release_ver");
				strNames.push_back("nightly_ver");
				strNames.push_back("release_url");
				strNames.push_back("nightly_url");

				strValues = internal_json_reader(json, val2, strNames);

				release_BS_ver = strValues[0];
				nightly_BS_ver = strValues[1];
				release_BS_url = strValues[2];
				nightly_BS_url = strValues[3];
				strValues.clear();
				strNames.clear();

				//struct stat st;

				// Download nds-bootstrap version data
				if (access("sdmc:/_nds/TWiLightMenu/release-bootstrap.ver", F_OK) == -1) {
					snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
						"Now downloading nds-bootstrap...\n"
						"(Release version data)"
						"\n"
						"Do not turn off the power.\n");
					pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
					pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
					pp2d_end_draw();

					FILE* ver = fopen("sdmc:/_nds/TWiLightMenu/release-bootstrap.ver", "w");
					if(!ver) {
						snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
							"Download failed.");
						for (int i = 0; i < 60; i++) {
							pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
							pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
							pp2d_end_draw();
						}
					}
					fputs(release_BS_ver.c_str(), ver);
					fclose(ver);
				}
				if (access("sdmc:/_nds/TWiLightMenu/nightly-bootstrap.ver", F_OK) == -1) {
					snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
						"Now downloading nds-bootstrap...\n"
						"(Nightly version data)"
						"\n"
						"Do not turn off the power.\n");
					pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
					pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
					pp2d_end_draw();

					FILE* ver = fopen("sdmc:/_nds/TWiLightMenu/nightly-bootstrap.ver", "w");
					if(!ver) {
						snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
							"Download failed.");
						for (int i = 0; i < 60; i++) {
							pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
							pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
							pp2d_end_draw();
						}
					}
					fputs(nightly_BS_ver.c_str(), ver);
					fclose(ver);
				}

				// Download nds-bootstrap .nds files
				if (access("sdmc:/_nds/nds-bootstrap-release.nds", F_OK) == -1) {
					snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
						"Now downloading nds-bootstrap...\n"
						"(Release)\n"
						"\n"
						"Do not turn off the power.\n");
					pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
					pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
					pp2d_end_draw();

					int res = downloadFile(release_BS_url.c_str(),"/_nds/nds-bootstrap-bootstrap.nds", MEDIA_SD_FILE);
					if (res != 0) {
						snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
							"Download failed.");
						for (int i = 0; i < 60; i++) {
							pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
							pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
							pp2d_end_draw();
						}
					}
				}
				if (access("sdmc:/_nds/nds-bootstrap-nightly.nds", F_OK) == -1) {
					snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
						"Now downloading nds-bootstrap...\n"
						"(Nightly)\n"
						"\n"
						"Do not turn off the power.\n");
					pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
					pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
					pp2d_end_draw();

					int res = downloadFile(nightly_BS_url.c_str(),"/_nds/nds-bootstrap-nightly.nds", MEDIA_SD_FILE);
					if (res != 0) {
						snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
							"Download failed.");
						for (int i = 0; i < 60; i++) {
							pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
							pp2d_draw_text(24, 32, 0.5f, 0.5f, WHITE, download_textOnScreen);
							pp2d_end_draw();
						}
					}
				}

			}
		}
	}
}

/**
 * Update nds-bootstrap to the latest nightly build.
 */
void UpdateBootstrapNightly(void) {
	snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
		"Now updating nds-bootstrap (Nightly)...");
	pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
	pp2d_draw_texture(loadingbgtex, 0, 0);
	pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, download_textOnScreen);
	pp2d_end_draw();
	// Download first .nds file
	rename("sdmc:/_nds/nds-bootstrap-nightly.nds", "sdmc:/_nds/nds-bootstrap-nightly.nds.launcherbak");
	int result = downloadFile(nightly_BS_url.c_str(),"/_nds/nds-bootstrap-nightly.nds", MEDIA_SD_FILE);
	
	// Then, download version string
	if(result == 0) {
		remove("sdmc:/_nds/nds-bootstrap-nightly.nds.launcherbak");
		remove("sdmc:/_nds/TWiLightMenu/nightly-bootstrap.ver");
		downloadBootstrapVersion(false);
		checkBootstrapVersion(false);
		snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
			"Done!");
		for (int i = 0; i < 60; i++) {
			pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
			pp2d_draw_texture(loadingbgtex, 0, 0);
			pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, download_textOnScreen);
			pp2d_end_draw();
		}
	} else {
		remove("sdmc:/_nds/nds-bootstrap-nightly.nds");
		rename("sdmc:/_nds/nds-bootstrap-nightly.nds.launcherbak", "sdmc:/_nds/nds-bootstrap-nightly.nds");
		snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
			"An error has occurred!");
		for (int i = 0; i < 60*2; i++) {
			pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
			pp2d_draw_texture(loadingbgtex, 0, 0);
			pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, download_textOnScreen);
			pp2d_end_draw();
		}
	}
}

/**
 * Update nds-bootstrap to the latest release build.
 */
void UpdateBootstrapRelease(void) {
	snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
		"Now updating nds-bootstrap (Release)...");
	pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
	pp2d_draw_texture(loadingbgtex, 0, 0);
	pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, download_textOnScreen);
	pp2d_end_draw();
	// Download first .nds file
	rename("sdmc:/_nds/nds-bootstrap-release.nds", "sdmc:/_nds/nds-bootstrap-release.nds.launcherbak");
	int result = downloadFile(release_BS_url.c_str(),"/_nds/nds-bootstrap-release.nds", MEDIA_SD_FILE);

	// Then, download version string
	if(result == 0) {
		remove("sdmc:/_nds/nds-bootstrap-release.nds.launcherbak");
		remove("sdmc:/_nds/TWiLightMenu/release-bootstrap.ver");
		downloadBootstrapVersion(true);
		checkBootstrapVersion(true);
		snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
			"Done!");
		for (int i = 0; i < 60; i++) {
			pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
			pp2d_draw_texture(loadingbgtex, 0, 0);
			pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, download_textOnScreen);
			pp2d_end_draw();
		}
	} else {
		remove("sdmc:/_nds/nds-bootstrap-release.nds");
		rename("sdmc:/_nds/nds-bootstrap-release.nds.launcherbak", "sdmc:/_nds/nds-bootstrap-release.nds");
		snprintf(download_textOnScreen, sizeof(download_textOnScreen), "%s",
			"An error has occurred!");
		for (int i = 0; i < 60*2; i++) {
			pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
			pp2d_draw_texture(loadingbgtex, 0, 0);
			pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, download_textOnScreen);
			pp2d_end_draw();
		}
	}
}

/**
 * Update nds-bootstrap to the latest build.
 */
void UpdateBootstrap(void) {
	UpdateBootstrapRelease();
	UpdateBootstrapNightly();
}

/**
 * Download bootstrap version files
 * @return non zero if error
 */

int downloadBootstrapVersion(bool type)
{
	u32 responseCode = 0;
	httpcContext context;	
	int res = -1;	
	
	httpcInit(0);
	if(R_FAILED(httpcOpenContext(&context, HTTPC_METHOD_GET, JSON_URL, 0))) {
		return -1;
	}
	if(R_FAILED(httpcAddRequestHeaderField(&context, "User-Agent", "TWiLightMenu"))) {
		return -1;
	}
	if(R_FAILED(httpcSetSSLOpt(&context, SSLCOPT_DisableVerify))) {
		return -1;
	}
	if(R_FAILED(httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED))) {
		return -1;
	}
	if(R_FAILED(httpcBeginRequest(&context))) {
		return -1;
	}
	if(R_FAILED(httpcGetResponseStatusCode(&context, &responseCode))) {
		return -1;
	}
    if (responseCode != 200) {
		return -1;
	}
	
	u32 size = 0;
	httpcGetDownloadSizeState(&context, NULL, &size);	
	char* jsonText = (char*) calloc(sizeof(char), size);
	if(jsonText != NULL) {
		u32 bytesRead = 0;
		http_read_internal(&context, &bytesRead, (u8*) jsonText, size);
		json_value* json = json_parse(jsonText, size);

		if(json != NULL) {
			if(json->type == json_object) {				
				
				json_value* val = json->u.object.values[4].value;				
				json_value* val2 = val->u.object.values[0].value;
				vector<std::string> strNames;
				vector<std::string> strValues;

				strNames.push_back("release_ver");
				strNames.push_back("nightly_ver");
				strNames.push_back("release_url");
				strNames.push_back("nightly_url");

				strValues = internal_json_reader(json, val2, strNames);
				
				release_BS_ver = strValues[0];
				nightly_BS_ver = strValues[1];
				release_BS_url = strValues[2];
				nightly_BS_url = strValues[3];
				strValues.clear();
				strNames.clear();			
			}
		}
	}
	
	free(jsonText);
	httpcCloseContext(&context);		

	FILE* ver = fopen(type ? "sdmc:/_nds/TWiLightMenu/release-bootstrap.ver" : "sdmc:/_nds/TWiLightMenu/nightly-bootstrap.ver", "w");
	if(!ver) {
		return res;
	}
	fputs(type ? release_BS_ver.c_str() : nightly_BS_ver.c_str(), ver);
	fclose(ver);

	return res;
}

/**
 * check, download and store bootstrap version
 */

void checkBootstrapVersion(bool type){
	FILE* VerFile = fopen(type ? "sdmc:/_nds/TWiLightMenu/release-bootstrap.ver" : "sdmc:/_nds/TWiLightMenu/nightly-bootstrap.ver", "r");
	if (!VerFile){
		if(checkWifiStatus()){
			downloadBootstrapVersion(type); // true == release
		}
	}
	fclose(VerFile);
}
