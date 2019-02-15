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
 * Get info from the GitHub API about a Release.
 * repo is where to get from. (Ex. "RocketRobz/TWiLightMenu")
 * item is that to get from the API. (Ex. "tag_name")
 * @return the string from the API.
 */
std::string getLatestRelease(std::string repo, std::string item);

/**
 * Get info from the GitHub API about a Commit.
 * repo is where to get from. (Ex. "RocketRobz/TWiLightMenu")
 * item is that to get from the API. (Ex. "sha")
 * @return the string from the API.
 */
std::string getLatestCommit(std::string repo, std::string item);

/**
 * Update nds-bootstrap to the latest build.
 */
void updateBootstrap(bool nightly);

/**
 * Update TWiLight Menu++ to the latest build.
 */
void updateTWiLight(bool nightly);

/**
 * Update the TWiLight Menu++ Updater to the latest build.
 */
void updateSelf(bool nightly);

/**
 * Update DeadSkullzJr's cheat DB to the latest version.
 */
void updateCheats(void);
