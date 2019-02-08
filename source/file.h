#pragma once

#include "common.hpp"

Result openFile(Handle* fileHandle, const char * path, bool write);
Result deleteFile(const char * path);
