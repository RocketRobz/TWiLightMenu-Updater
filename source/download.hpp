#pragma once

#include "common.hpp"

#define APP_TITLE "TWLMenu Updater"
#define VERSION_STRING "3.0.0"

enum DownloadError {
	DL_ERROR_NONE = 0,
	DL_ERROR_WRITEFILE,
	DL_ERROR_ALLOC,
	DL_ERROR_STATUSCODE,
	DL_ERROR_GIT,
};

Result downloadToFile(std::string url, std::string path);
Result downloadFromRelease(std::string url, std::string asset, std::string path);

/**
 * Check Wi-Fi status.
 * @return True if Wi-Fi is connected; false if not.
 */
bool checkWifiStatus(void);

/**
 * Update nds-bootstrap to the latest nightly build.
 */
void UpdateBootstrapNightly(void);

/**
 * Update nds-bootstrap to the latest release build.
 */
void UpdateBootstrapRelease(void);

/**
 * Update nds-bootstrap to the latest build.
 */
void updateBootstrap(void);

/**
 * Update TWiLight Menu++ to the latest build.
 */
void updateTWiLight(void);

/**
 * Update the TWiLight Menu++ Updater to the latest build.
 */
void updateSelf(void);
