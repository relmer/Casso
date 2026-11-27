#pragma once

#include "Pch.h"

struct ImageContents;





////////////////////////////////////////////////////////////////////////////////
//
//  IImageRecords
//
//  What a disk image's volume directory contains, stored by the image's
//  content, so a later read of the same bytes, in this run or a later one,
//  doesn't read and decode the image again. A change to the image changes
//  its key, so a record is never used for bytes that aren't there.
//
//  Whether the image is a game's saved game is in the same record, since
//  finding out decodes the image too.
//
////////////////////////////////////////////////////////////////////////////////

class IImageRecords
{
public:
    enum class SavedGame
    {
        Unknown,
        None,
        Present,
    };

    virtual ~IImageRecords () = default;

    virtual bool       FindContents   (const std::wstring & path, ImageContents & outContents) = 0;
    virtual void       StoreContents  (const std::wstring & path, const ImageContents & contents) = 0;
    virtual SavedGame  FindSavedGame  (const std::wstring & path) = 0;
    virtual void       StoreSavedGame (const std::wstring & path, bool hasSavedGame) = 0;

    //  Deletes the image's record, so its next read decodes the image again.
    virtual void       Remove         (const std::wstring & path) = 0;
};
