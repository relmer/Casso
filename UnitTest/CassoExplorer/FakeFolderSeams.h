#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderThumbnails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeFolderSeams
//
//  The files of each folder, the images with a thumbnail, each image
//  queued, and each folder listed. An image is marked queued by its path,
//  size and time.
//
////////////////////////////////////////////////////////////////////////////////

struct FakeFolderSeams
{
    std::map<std::wstring, std::vector<FolderThumbnails::ImageFile>>  files;
    std::set<std::wstring>                                             thumbnails;
    std::set<std::wstring>                                             marked;
    std::vector<std::wstring>                                          queued;
    std::vector<std::wstring>                                          checked;    // each path whose thumbnail was looked for
    std::vector<FolderThumbnails::FolderRequest>                       listed;
    int                                                                removals = 0;
    bool                                                               enabled  = true;



    FolderThumbnails::Seams  MakeSeams()
    {
        FolderThumbnails::Seams  seams;



        seams.list         = [this] (const FolderThumbnails::FolderRequest & request) { listed.push_back (request); return files[request.folder]; };
        seams.hasThumbnail = [this] (const std::wstring & path) { checked.push_back (path); return thumbnails.contains (path); };
        seams.markQueued   = [this] (const FolderThumbnails::ImageFile & image) { return marked.insert (std::format (L"{}|{}|{}", image.path, image.bytes, image.time)).second; };
        seams.queue        = [this] (const std::wstring & path) { queued.push_back (path); };
        seams.removeQueued = [this]() { removals++; };
        seams.isEnabled    = [this]() { return enabled; };

        return seams;
    }
};
