#pragma once

// Resource IDs for Casso

// Menu IDs
#define IDM_FILE_OPEN               40001
#define IDM_FILE_RECENT             40002
#define IDM_FILE_EXIT               40003

#define IDM_EDIT_COPY_TEXT          40005
#define IDM_EDIT_COPY_SCREENSHOT    40006
#define IDM_EDIT_PASTE              40007

#define IDM_MACHINE_RESET           40010
#define IDM_MACHINE_POWERCYCLE      40011
#define IDM_MACHINE_PAUSE           40012
#define IDM_MACHINE_STEP            40013
#define IDM_MACHINE_SPEED_1X        40014
#define IDM_MACHINE_SPEED_2X        40015
#define IDM_MACHINE_SPEED_MAX       40016
#define IDM_MACHINE_INFO            40017

#define IDM_DISK_INSERT1            40020
#define IDM_DISK_INSERT2            40021
#define IDM_DISK_EJECT1             40022
#define IDM_DISK_EJECT2             40023
#define IDM_DISK_WRITEMODE_BUFFER   40024
#define IDM_DISK_WRITEMODE_COW      40025
#define IDM_DISK_WRITEPROTECT1      40026
#define IDM_DISK_WRITEPROTECT2      40027

#define IDM_VIEW_COLOR              40030
#define IDM_VIEW_GREEN              40031
#define IDM_VIEW_AMBER              40032
#define IDM_VIEW_WHITE              40033
#define IDM_VIEW_FULLSCREEN         40034
#define IDM_VIEW_CRT_SHADER         40035
#define IDM_VIEW_RESET_SIZE         40036
#define IDM_VIEW_DISKII_DEBUG       40038
#define IDM_VIEW_SETTINGS           40039  // Consolidated settings panel

#define IDM_AUDIO_DRIVE_ENABLE      40050
#define IDM_AUDIO_DRIVE_DISABLE     40051
#define IDM_AUDIO_DRIVE_MECHANISM   40052

#define IDM_HELP_KEYMAP             40040
#define IDM_HELP_ABOUT              40042

// Accelerator table
#define IDR_ACCELERATOR             101

// Application icons. Windows uses the icon with the lowest numerical
// resource ID as the EXE's shell icon, so the default app icon
// (plain silhouette — most legible at small sizes) gets the lowest
// ID. The remaining icons are embedded for completeness so callers
// can switch via LoadIcon.
#define IDI_CASSO_SILHOUETTE        102
#define IDI_CASSO_SILHOUETTE_ACCENT 103
#define IDI_CASSO_SILHOUETTE_RAINBOW 104
#define IDI_CASSO_FLAT_COLOR_HEAD   105
#define IDI_CASSO_PHOTOREAL         106
#define IDI_CASSO                   IDI_CASSO_SILHOUETTE

// Embedded default machine configs (RCDATA) — extracted to disk on
// first run when the user has no Machines/ folder.
#define IDR_MACHINE_APPLE2          200
#define IDR_MACHINE_APPLE2PLUS      201
#define IDR_MACHINE_APPLE2E         202

// Embedded built-in theme files (RCDATA). Per spec 007-ui-overhaul
// The three built-in themes are extracted to
// <assetBase>/Themes/<Name>/ by AssetBootstrap::EnsureThemes on
// first run + upgrade. Resource IDs are blocked per theme so the
// asset bootstrap can iterate them via a single table.
//
//   Skeuomorphic:   300..319
//   DarkModern:     320..339
//   RetroTerminal:  340..359
#define IDR_THEME_SKEUO_THEME_JSON          300
#define IDR_THEME_SKEUO_FONT_TTF            310
#define IDR_THEME_SKEUO_FONT_OFL            311
#define IDR_THEME_SKEUO_FONT_TODO           312

#define IDR_THEME_DARK_THEME_JSON           320
#define IDR_THEME_DARK_FONT_TTF             330
#define IDR_THEME_DARK_FONT_OFL             331
#define IDR_THEME_DARK_FONT_TODO            332

#define IDR_THEME_RETRO_THEME_JSON          340
#define IDR_THEME_RETRO_FONT_TTF            350
#define IDR_THEME_RETRO_FONT_OFL            351
#define IDR_THEME_RETRO_FONT_TODO           352
