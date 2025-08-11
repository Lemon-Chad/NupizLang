
#ifndef jp_common_h
#define jp_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef WIN32
#include <io.h>
#include <direct.h>
#define F_OK 0
#define access _access
#define getcwd _getcwd
#endif

#ifndef WIN32
#include <unistd.h>
#endif

//#define DEBUG_PRINT_DUMPER
//#define DEBUG_PRINT_LOADER

//#define DEBUG_PRINT_CODE
//#define DEBUG_TRACE_EXECUTION

//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC
//#define DEBUG_SLOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)

#endif
