#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Ui/DiskInspector/InspectorTables.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapText
//
//  The File map tab's words (FR-086 to FR-095): each role as its file
//  system calls it, a cell's tooltip with the sector that holds it and each
//  owner's place ("HELLO, data sector 3 of 5"), the file list's rows and
//  states, why a disk is not mapped, and "Copy map": a row per track, a
//  letter per sector, and the key.
//
////////////////////////////////////////////////////////////////////////////////

class FileMapText
{
public:
    static vector<std::wstring>  GetFileColumns (const FileMap & map);
    static vector<TableRow>      BuildFileRows  (const FileMap & map, bool isShowingDeleted, bool isBadOnly, int sortColumn, bool isDescending);

    static std::wstring  FormatRole      (MapFileSystem fileSystem, SectorRole role);
    static std::wstring  FormatResult    (SectorResult result);
    static std::wstring  FormatCell      (const FileMap & map, int cell);
    static std::wstring  FormatOwners    (const FileMap & map, int cell);
    static std::wstring  FormatTooltip   (const FileMap & map, int cell);
    static std::wstring  FormatStates    (const MappedFile & file);
    static std::wstring  FormatSize      (const FileMap & map, const MappedFile & file);
    static std::wstring  FormatNotMapped (const FileMap & map);
    static std::wstring  FormatVolume    (const FileMap & map);
    static std::wstring  FormatMap       (const FileMap & map);
    static wchar_t       GetRoleLetter   (SectorRole role);
};
