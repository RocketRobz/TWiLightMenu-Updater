#pragma once

#include <3ds.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "file.h"

#ifdef __cplusplus
}

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <curl/curl.h>

#include "stringutils.hpp"
#include "json.hpp"

using json = nlohmann::json;

#endif

extern char * arg0;

#define WORKING_DIR       "/3ds/"

#define HBL_FILE_NAME     APP_TITLE  ".3dsx"
#define HBL_FILE_PATH     WORKING_DIR  "/"  HBL_FILE_NAME

#define CONFIG_FILE_NAME  "config.json"
#define CONFIG_FILE_PATH  WORKING_DIR  "/"  CONFIG_FILE_NAME

#define CONFIG_FILE_URL   "https://raw.githubusercontent.com/LiquidFenrir/MultiUpdater/rewrite/config.json"
