#include "Pch.h"

#include "CassoExplorer/Model/DiskImagePictures.h"
#include "Core/TextEncoding.h"
#include "Devices/Printer/PngCodec.h"
#include "DiskCheck/BootCheck.h"





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
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiIconImage> DiskImagePictures::Load (const std::wstring & path, int sizePx)
{
    RgbaImage        picture;
    ThumbnailRecord  record;
    bool             isUserSet = false;



    return Read (path, picture, record, isUserSet) ? Scale (picture, sizePx) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::Find
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImagePictures::Find (const std::wstring & path, std::shared_ptr<const DxuiIconImage> & outPicture, std::wstring & outResult)
{
    RgbaImage                       picture;
    ThumbnailRecord                 record;
    bool                            isUserSet = false;
    bool                            found     = Read (path, picture, record, isUserSet);
    std::shared_ptr<DxuiIconImage>  image;



    outPicture.reset();
    outResult.clear();

    if (found)
    {
        image         = std::make_shared<DxuiIconImage>();
        image->width  = picture.width;
        image->height = picture.height;
        image->bgraPremul.resize ((size_t) picture.width * picture.height);

        for (size_t i = 0; i < image->bgraPremul.size(); i++)
        {
            image->bgraPremul[i] = 0xFF000000u | ((uint32_t) picture.rgba[i * 4] << 16) | ((uint32_t) picture.rgba[i * 4 + 1] << 8) | picture.rgba[i * 4 + 2];
        }

        outPicture = image;
        outResult  = DescribeRecord (record, isUserSet);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::DescribeRecord
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskImagePictures::DescribeRecord (const ThumbnailRecord & record, bool isUserSet)
{
    std::string  machine = BootCheck::GetMachineName (record.machine);
    std::string  text;



    if (isUserSet)
    {
        text = "The picture was set in Casso.";
    }
    else if (record.verdict == BootProbe::GetVerdictName (BootVerdict::Booted))
    {
        text = std::format ("Boots on the {}.", machine);
    }
    else if (record.verdict == BootProbe::GetVerdictName (BootVerdict::DidNotBoot))
    {
        text = std::format ("Did not boot on the {}. {}", machine, record.reason);
    }
    else if (record.verdict == BootProbe::GetVerdictName (BootVerdict::NotBootable))
    {
        text = std::format ("Not a bootable disk. {}", record.reason);
    }
    else if (!record.verdict.empty())
    {
        text = std::format ("It is unclear whether it boots on the {}. {}", machine, record.reason);
    }

    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures::Read
//
//  An image is queued once for each size and time its file has, so an image
//  that can't be booted isn't booted again each time its folder is drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImagePictures::Read (const std::wstring & path, RgbaImage & outPicture, ThumbnailRecord & outRecord, bool & outIsUserSet)
{
    HRESULT                hr       = S_OK;
    std::filesystem::path  file     = path;
    std::error_code        failed;
    uint64_t               bytes    = std::filesystem::file_size (file, failed);
    bool                   isFloppy = !failed && IsFloppySize (file.extension().wstring(), bytes);
    int64_t                time     = 0;
    std::vector<Byte>      png;
    bool                   isNew    = false;
    bool                   found    = false;



    BAIL_OUT_IF (!isFloppy, S_OK);

    hr = m_store.Find (file, png, outRecord, outIsUserSet);
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

    hr = PngCodec::DecodeRgba (png, outPicture);
    CHR (hr);

    found = true;

Error:
    return found;
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
