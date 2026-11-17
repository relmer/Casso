#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "Shell/DiskMru.h"

class IDiskFileIo;
class IFileSystem;
class ImageClassCache;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoImagesListing
//
//  The Casso node's list: every disk and tape image in the folders under
//  the node, without their subfolders, one row each with its folder and the
//  time Casso last loaded it. A folder that is missing or can't be read is
//  skipped, and a file reached through two of the folders is listed once.
//
////////////////////////////////////////////////////////////////////////////////

class CassoImagesListing
{
public:
    static void  Build (IFileSystem                        & fs,
                        IDiskFileIo                        & fileIo,
                        ImageClassCache                    & cache,
                        const std::vector<std::wstring>    & folders,
                        const std::vector<DiskMru::Entry>  & recent,
                        std::vector<CatalogRow>            & outRows);

    //  The recent list's images and load times, read from the user's
    //  preferences under `baseDir`; empty when they can't be read.
    static std::vector<DiskMru::Entry>  LoadRecent (IFileSystem & fs, const std::wstring & baseDir);

    //  The key two spellings of one path have in common.
    static std::wstring  MakePathKey (const std::wstring & path);
};
