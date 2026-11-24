#pragma once

#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/Preview/IPreviewRefresh.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/HostFolderReader.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Core/MemoryBus.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/ImageClassCache.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPartsFixture
//
//  The browser's parts and the preview's parts over an in-memory file system
//  and a fake disk file I/O, wired as CassoExplorerBrowser wires them, for
//  testing one previewer alone. The view choices call a refresh that counts
//  its calls.
//
////////////////////////////////////////////////////////////////////////////////

struct BrowserPartsFixture
{
    class FakePreviewRefresh : public IPreviewRefresh
    {
    public:
        void  UpdatePreview () override { updates++; }

        int  updates = 0;
    };

    InMemoryFileSystem          fs;
    FakeDiskFileIo              io;
    ImageClassCache             imageClasses;
    HostFolderReader            hostFolders;
    TreeModel                   tree         { fs, hostFolders };
    BrowserModel                model;
    DiskOperations              operations   { io };
    DiskImageCatalog            catalog      { operations };
    CassoExplorerShellListings  shellListings;
    BrowserListState            list;
    BrowserSources              sources;
    BrowserParts                parts        { fs, io, imageClasses, catalog, tree, hostFolders, model, operations, shellListings, list, sources };

    MemoryBus                   bus;
    PreviewContent              preview;
    FakePreviewRefresh          refresh;
    PreviewViewState            viewState    { refresh };
    PictureMonitorChoice        monitor      { preview, bus };
    PreviewParts                previewParts { preview, bus, viewState, monitor };

    //  One tab open on a folder, as a browser has, for the cached catalogs.
    BrowserPartsFixture()
    {
        model.OpenTab (Location::MakeHostFolder (L"C:\\Disks"));
    }

    //  A disk image or other file the disk file I/O reads.
    void  Seed (const char * path, const std::vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 100 };
    }

    //  A file the file system reads.
    void  SeedHostFile (const wchar_t * path, const std::string & content)
    {
        AssertSucceeded (fs.WriteAllText (path, content));
    }
};
