#pragma once
#include "cuddObj.hh"
#include "cuddInt.h"

int PreReorderHook( DdManager *dd, const char *str, void *data);
int PostReorderHook( DdManager *dd, const char *str, void *data);
int PreGarbageCollectHook(DdManager* dd, const char* str, void* data);
int PostGarbageCollectHook(DdManager* dd, const char* str, void* data);
