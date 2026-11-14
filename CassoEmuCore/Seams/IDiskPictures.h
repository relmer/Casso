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

    //  The picture at the size it was made, and a line with the image's boot
    //  result. False with no picture yet. Called on a window's thread.
    virtual bool  Find (const std::wstring & path, std::shared_ptr<const DxuiIconImage> & outPicture, std::wstring & outResult) = 0;

    //  A window that is posted `message` when a picture that was missing
    //  has been made, until it is removed. The message's wParam is
    //  kPicturesChanged when pictures already drawn were changed or removed.
    virtual void  AddTarget    (HWND hwnd, UINT message) = 0;
    virtual void  RemoveTarget (HWND hwnd)               = 0;

    static constexpr WPARAM  kPicturesChanged = 1;

    //  Whether the image's thumbnail is one the user set, and its removal,
    //  after which the thumbnail made by booting is drawn.
    virtual bool     IsUserSet     (const std::wstring & path) = 0;
    virtual HRESULT  RemoveUserSet (const std::wstring & path) = 0;

    //  The keys of the thumbnails of files to be deleted, and the removal of
    //  those thumbnails once the files are gone.
    virtual std::vector<std::string>  GetKeys       (const std::vector<std::wstring> & paths) = 0;
    virtual void                      RemoveDeleted (const std::vector<std::string> & keys)   = 0;

    //  The count of thumbnails the user set, and the removal of every
    //  thumbnail made by booting, and with includeUserSet those too.
    virtual int      CountUserSet () = 0;
    virtual HRESULT  Clear        (bool includeUserSet) = 0;
};
