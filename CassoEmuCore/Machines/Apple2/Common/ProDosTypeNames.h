#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosTypeNames
//
//  The three-letter abbreviations of the ProDOS file types in Apple's File
//  Type Notes, as they appear in a catalog. A type with no abbreviation is
//  written as its number.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosTypeNames
{
public:
    //  The abbreviation, or null for a type with no abbreviation.
    static const wchar_t  * Find     (Byte type);

    //  The abbreviation, or the type as `$XX`.
    static std::wstring     Format   (Byte type);

    //  A type from its abbreviation in any case, or from its number as `$C0`
    //  or `0xC0`.
    static bool             TryParse (const std::string & text, Byte & outType);

private:
    struct Row
    {
        Byte             type;
        const wchar_t  * abbreviation;
    };

    static constexpr Row  kRows[] =
    {
        { 0x00, L"NON" }, { 0x01, L"BAD" }, { 0x02, L"PCD" }, { 0x03, L"PTX" },
        { 0x04, L"TXT" }, { 0x05, L"PDA" }, { 0x06, L"BIN" }, { 0x07, L"FNT" },
        { 0x08, L"FOT" }, { 0x09, L"BA3" }, { 0x0A, L"DA3" }, { 0x0B, L"WPF" },
        { 0x0C, L"SOS" }, { 0x0F, L"DIR" }, { 0x10, L"RPD" }, { 0x11, L"RPI" },
        { 0x12, L"AFD" }, { 0x13, L"AFM" }, { 0x14, L"AFR" }, { 0x15, L"SCL" },
        { 0x16, L"PFS" }, { 0x19, L"ADB" }, { 0x1A, L"AWP" }, { 0x1B, L"ASP" },
        { 0x20, L"TDM" }, { 0x21, L"IPS" }, { 0x22, L"UPV" }, { 0x29, L"3SD" },
        { 0x2A, L"8SC" }, { 0x2B, L"8OB" }, { 0x2C, L"8IC" }, { 0x2D, L"8LD" },
        { 0x2E, L"P8C" }, { 0x41, L"OCR" }, { 0x42, L"FTD" }, { 0x50, L"GWP" },
        { 0x51, L"GSS" }, { 0x52, L"GDB" }, { 0x53, L"DRW" }, { 0x54, L"GDP" },
        { 0x55, L"HMD" }, { 0x56, L"EDU" }, { 0x57, L"STN" }, { 0x58, L"HLP" },
        { 0x59, L"COM" }, { 0x5A, L"CFG" }, { 0x6B, L"BIO" }, { 0x6D, L"DVR" },
        { 0xA0, L"WP " }, { 0xAB, L"GSB" }, { 0xAC, L"TDF" }, { 0xAD, L"BDF" },
        { 0xB0, L"SRC" }, { 0xB1, L"OBJ" }, { 0xB2, L"LIB" }, { 0xB3, L"S16" },
        { 0xB4, L"RTL" }, { 0xB5, L"EXE" }, { 0xB6, L"PIF" }, { 0xB7, L"TIF" },
        { 0xB8, L"NDA" }, { 0xB9, L"CDA" }, { 0xBA, L"TOL" }, { 0xBB, L"DVR" },
        { 0xBC, L"LDF" }, { 0xBD, L"FST" }, { 0xBF, L"DOC" }, { 0xC0, L"PNT" },
        { 0xC1, L"PIC" }, { 0xC2, L"ANI" }, { 0xC3, L"PAL" }, { 0xC5, L"OOG" },
        { 0xC6, L"SCR" }, { 0xC7, L"CDV" }, { 0xC8, L"FON" }, { 0xC9, L"FND" },
        { 0xCA, L"ICN" }, { 0xD5, L"MUS" }, { 0xD6, L"INS" }, { 0xD7, L"MDI" },
        { 0xD8, L"SND" }, { 0xDB, L"DBM" }, { 0xE0, L"LBR" }, { 0xE2, L"ATK" },
        { 0xEE, L"R16" }, { 0xEF, L"PAS" }, { 0xF0, L"CMD" }, { 0xF9, L"OS"  },
        { 0xFA, L"INT" }, { 0xFB, L"IVR" }, { 0xFC, L"BAS" }, { 0xFD, L"VAR" },
        { 0xFE, L"REL" }, { 0xFF, L"SYS" },
    };
};
