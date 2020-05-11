#pragma once

#include "common.hpp"

Result makeDirs(const char * path);
Result openFile(Handle* fileHandle, const char * path, bool write);
Result deleteFile(const char * path);
Result removeDir(const char * path);
Result removeDirRecursive(const char * path);