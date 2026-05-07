#pragma once

// Version information for Casso
// The build number and year are automatically updated by the pre-build script

#define VERSION_MAJOR 1
#define VERSION_MINOR 3
#define VERSION_BUILD 420
#define VERSION_YEAR 2026

// Helper macros for stringification
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

// Full version string (e.g., "1.0.1")
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_BUILD)

// Build timestamp (uses compiler's __DATE__ and __TIME__)
#define VERSION_BUILD_TIMESTAMP __DATE__ " " __TIME__

// Current year as string (e.g., "2026")
#define VERSION_YEAR_STRING STRINGIFY(VERSION_YEAR)
