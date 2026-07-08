#pragma once

// Version information for Casso
//
// Semantic versioning: MAJOR.MINOR.PATCH. All three are bumped
// manually. VERSION_YEAR tracks the copyright year. There is no
// auto-incrementing build counter -- VERSION_BUILD_TIMESTAMP below
// identifies an individual compile when that granularity is needed.

#define VERSION_MAJOR 1
#define VERSION_MINOR 6
#define VERSION_PATCH 1
#define VERSION_YEAR 2026

// Helper macros for stringification
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

// Full version string (e.g., "1.6.0")
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

// Build timestamp (uses compiler's __DATE__ and __TIME__)
#define VERSION_BUILD_TIMESTAMP __DATE__ " " __TIME__

// Current year as string (e.g., "2026")
#define VERSION_YEAR_STRING STRINGIFY(VERSION_YEAR)
