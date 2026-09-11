#pragma once

#include "Pch.h"

#include "Cassque/CassqueBrowser.h"
#include "Cassque/Model/DragPayload.h"
#include "Cassque/Model/HostFileNaming.h"
#include "Window/DxuiDragDropSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueDragOut
//
//  What a drag out of the list carries, as the formats the drag source
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

class CassqueDragOut
{
public:
    //  The formats for the browser's current selection; empty when there is
    //  nothing to drag.
    static std::vector<DxuiDragDropSource::Format>  BuildFormats (CassqueBrowser & browser, HostFileNaming::Style style);

    //  A FILEGROUPDESCRIPTORW holding one descriptor per entry.
    static std::vector<uint8_t>  MakeFileGroupDescriptor (const std::vector<DragPayload::Descriptor> & descriptors);

    //  A DROPFILES block with its double-terminated wide path list.
    static std::vector<uint8_t>  MakeHDrop (const std::vector<std::wstring> & paths);

    //  The conversion a descriptor's host name asks for.
    static DiskOperations::Encoding  GetEncoding (const DragPayload::Descriptor & descriptor);
};
