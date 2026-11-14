#include "Pch.h"

#include "CassoExplorer/Model/DiskImagePictures.h"
#include "Devices/Printer/PngCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::DiskImagePictures
//
////////////////////////////////////////////////////////////////////////////////

DiskImagePictures::DiskImagePictures (ThumbnailStore & store, QueueFn queue) :
    m_store (store),
    m_queue (std::move (queue))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::Load
//
//  An image is queued once for each size and time its file has, so an image
//  that can't be booted isn't booted again each time its folder is drawn.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> DiskImagePictures::Load (const std::wstring & path, int sizePx)
{
    HRESULT                         hr        = S_OK;
    std::filesystem::path           file      = path;
    std::error_code                 failed;
    uint64_t                        bytes     = std::filesystem::file_size (file, failed);
    bool                            isFloppy  = !failed && IsFloppySize (file.extension().wstring(), bytes);
    int64_t                         time      = 0;
    std::vector<Byte>               png;
    ThumbnailRecord                 record;
    bool                            isUserSet = false;
    bool                            isNew     = false;
    RgbaImage                       picture;
    std::shared_ptr<DxuiIconImage>  image;



    BAIL_OUT_IF (!isFloppy, S_OK);

    hr = m_store.Find (file, png, record, isUserSet);
    CHR (hr);

    if (png.empty())
    {
        time = (int64_t) std::filesystem::last_write_time (file, failed).time_since_epoch().count();

        {
            std::lock_guard  lock (m_lock);

            isNew = m_queued.insert (std::format (L"{}|{}|{}", path, bytes, time)).second;
        }

        if (isNew && m_queue)
        {
            m_queue (path);
        }

        BAIL_OUT_IF (true, S_OK);
    }

    hr = PngCodec::DecodeRgba (png, picture);
    CHR (hr);

    image = Scale (picture, sizePx);

Error:
    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::AddTarget
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePictures::AddTarget (HWND hwnd, UINT message)
{
    std::lock_guard  lock (m_lock);



    std::erase_if (m_targets, [hwnd] (const Target & each) { return each.hwnd == hwnd; });
    m_targets.push_back ({ hwnd, message });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::RemoveTarget
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePictures::RemoveTarget (HWND hwnd)
{
    std::lock_guard  lock (m_lock);



    std::erase_if (m_targets, [hwnd] (const Target & each) { return each.hwnd == hwnd; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::NotifyReady
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePictures::NotifyReady()
{
    std::lock_guard  lock (m_lock);



    for (const Target & target : m_targets)
    {
        PostMessageW (target.hwnd, target.message, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::ForgetQueued
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePictures::ForgetQueued()
{
    std::lock_guard  lock (m_lock);



    m_queued.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::IsFloppySize
//
//  A sector or nibble image of a 5.25" disk is under a quarter of a
//  megabyte; a WOZ of one with flux tracks is larger. An 800K or hard disk
//  image isn't booted.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImagePictures::IsFloppySize (const std::wstring & extension, uint64_t bytes)
{
    bool  isWoz = _wcsicmp (extension.c_str(), L".woz") == 0;



    return bytes > 0 && bytes <= (isWoz ? s_kMaxWozBytes : s_kMaxSectorBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::Scale
//
//  Each pixel is the average of the picture's pixels under it.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> DiskImagePictures::Scale (const RgbaImage & picture, int sizePx)
{
    std::shared_ptr<DxuiIconImage>  image;
    int                             width   = sizePx;
    int                             height  = sizePx * 3 / 4;
    int                             top     = (sizePx - height) / 2;
    bool                            isValid = sizePx > 0 && picture.width > 0 && picture.height > 0
                                              && picture.rgba.size() == (size_t) picture.width * picture.height * 4;



    if (!isValid)
    {
        return nullptr;
    }

    image         = std::make_shared<DxuiIconImage>();
    image->width  = sizePx;
    image->height = sizePx;
    image->bgraPremul.assign ((size_t) sizePx * sizePx, 0);

    for (int y = 0; y < height; y++)
    {
        int  firstRow = y * picture.height / height;
        int  lastRow  = std::max (firstRow + 1, (y + 1) * picture.height / height);

        for (int x = 0; x < width; x++)
        {
            int       firstColumn = x * picture.width / width;
            int       lastColumn  = std::max (firstColumn + 1, (x + 1) * picture.width / width);
            uint32_t  red         = 0;
            uint32_t  green       = 0;
            uint32_t  blue        = 0;
            uint32_t  count       = (uint32_t) ((lastRow - firstRow) * (lastColumn - firstColumn));

            for (int row = firstRow; row < lastRow; row++)
            {
                const Byte  * pixel = &picture.rgba[((size_t) row * picture.width + firstColumn) * 4];

                for (int column = firstColumn; column < lastColumn; column++, pixel += 4)
                {
                    red   += pixel[0];
                    green += pixel[1];
                    blue  += pixel[2];
                }
            }

            image->bgraPremul[(size_t) (top + y) * sizePx + x] = 0xFF000000u | ((red / count) << 16) | ((green / count) << 8) | (blue / count);
        }
    }

    return image;
}
