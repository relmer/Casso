#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "CassoExplorer/Model/HostFileNaming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut
//
//  What a drag out of the list holds, as the formats the drag source
//  offers.
//
//  Files in an image go as file descriptors plus contents, which Explorer
//  and most other targets read, and as the private catalog-entry format a
//  drop on another image reads to copy raw. Each file's contents are read
//  from the image only when the target asks for that index, with the
//  conversion its descriptive name promises. Disk images in a host folder go
//  as an ordinary file drop.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerDragOut
{
public:
    //  The formats for the browser's current selection; empty when there is
    //  nothing to drag.
    static std::vector<DxuiDragDropSource::Format>  BuildFormats (CassoExplorerBrowser & browser, HostFileNaming::Style style);

    //  The same formats on the clipboard, for a paste into Explorer. A drag
    //  reads each file when the target asks, while it still holds the
    //  selection it began with; a paste can come long after the selection
    //  moved, so every file is read before the clipboard is set.
    static HRESULT  CopyToClipboard (CassoExplorerBrowser & browser, HostFileNaming::Style style);

    //  Why CopyToClipboard failed, as the end of a sentence: the clipboard
    //  in use, the Apple gone while its files were read, or the system's
    //  text for any other error.
    static std::wstring  DescribeCopyFailure (HRESULT hr);

    //  The selected files written to a folder of their own under the user's
    //  temporary folder, as Explorer opens a file inside a zip: each by the
    //  name a copy out would give it.
    static HRESULT  WriteToTempFolder (CassoExplorerBrowser & browser, HostFileNaming::Style style, std::vector<std::wstring> & outPaths);

    //  A FILEGROUPDESCRIPTORW holding one descriptor per entry.
    static std::vector<uint8_t>  MakeFileGroupDescriptor (const std::vector<DragPayload::Descriptor> & descriptors);

    //  A DROPFILES block with its double-terminated wide path list.
    static std::vector<uint8_t>  MakeHDrop (const std::vector<std::wstring> & paths);

    //  The formats for files selected on a linked Apple's drive.
    static std::vector<DxuiDragDropSource::Format>  BuildAppleDriveFormats (CassoExplorerBrowser & browser, HostFileNaming::Style style);

    //  How long a drop waits for each file from an Apple.
    static constexpr DWORD  kDropWaitMs = 60000;

    //  A whole disk takes far longer to come over the link than a file.
    static constexpr DWORD  kDiskDropWaitMs = 180000;

    //  Waits up to ms between a drop's checks on the link. Paints and posted
    //  messages are run meanwhile, so the window is redrawn while a drop onto
    //  Explorer waits on the link. Input is queued until the drop finishes.
    static void  PumpWhileWaiting (DWORD ms);

    //  The format a linked Apple's drive drags with, the drive's id in it,
    //  so a drop inside Casso Explorer reads the disk without waiting on it.
    static constexpr const wchar_t *  kDriveFormatName = L"CassoExplorer.AppleDrive";

    //  A linked Apple's whole drive as one image file, its volume in its name:
    //  the drive's id for Casso Explorer's own drops, and the image's bytes,
    //  read over the link when a drop elsewhere requests them.
    static std::vector<DxuiDragDropSource::Format>  BuildDiskFormats (CassoExplorerBrowser & browser, const std::wstring & driveId);

    //  The drive id a drag of a whole drive has, or false for any other drag.
    static bool  TryGetDraggedDrive (IDataObject * data, std::wstring & outDriveId);

    //  The conversion a descriptor's host name asks for.
    static DiskOperations::Encoding  GetEncoding (const DragPayload::Descriptor & descriptor);
};
