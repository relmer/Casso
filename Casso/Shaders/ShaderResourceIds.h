#pragma once

// Embedded shader source resources. Kept away from app/menu IDs.
//
// Only the settings compositor still reads its HLSL at run time. The CRT
// chain and the blit pair are compiled by fxc at build time and reach the
// renderer as bytecode, so they need no resource id at all.
#define IDR_HLSL_SETTINGS_GAUSSIAN_H 4009
#define IDR_HLSL_SETTINGS_GAUSSIAN_V 4010
#define IDR_HLSL_SETTINGS_COMPOSE    4011
