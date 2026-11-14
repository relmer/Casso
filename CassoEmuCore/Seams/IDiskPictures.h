#pragma once

#include "Pch.h"

struct DxuiIconImage;





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskPictures
//
//  The picture of a disk image's screen once booted, drawn in place of the
//  floppy icon where a list draws thumbnails.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskPictures
{
public:
    virtual ~IDiskPictures () = default;

    //  The picture in a square of sizePx, centered, or null with no picture
    //  yet. Called on an icon loader's thread.
    virtual std::shared_ptr<DxuiIconImage>  Load (const std::wstring & path, int sizePx) = 0;

    //  A window that is posted `message` when a picture that was missing
    //  has been made, until it is removed.
    virtual void  AddTarget    (HWND hwnd, UINT message) = 0;
    virtual void  RemoveTarget (HWND hwnd)               = 0;
};
