#pragma once

#include "Pch.h"

#include "Devices/Printer/RgbaImage.h"
#include "DiskCheck/ThumbnailStore.h"
#include "Seams/IDiskPictures.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePictures
//
//  IDiskPictures from the thumbnail store. An image with no thumbnail is
//  passed to the queue function once, to be booted, and has no picture
//  until NotifyReady is called after its boot.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImagePictures : public IDiskPictures
{
public:
    using QueueFn = std::function<void (const std::wstring & path)>;

    DiskImagePictures (ThumbnailStore & store, QueueFn queue);

    std::shared_ptr<DxuiIconImage>  Load (const std::wstring & path, int sizePx) override;
    bool                            Find (const std::wstring & path, std::shared_ptr<const DxuiIconImage> & outPicture, std::wstring & outResult) override;

    void  AddTarget    (HWND hwnd, UINT message) override;
    void  RemoveTarget (HWND hwnd) override;

    //  Posts each target its message.
    void  NotifyReady  ();

    //  Every image queued before can be queued again, as after ROMs are
    //  installed.
    void  ForgetQueued ();

    //  The picture scaled to sizePx wide and three quarters as tall, the
    //  proportions of an Apple's screen, centered in a square of sizePx with
    //  the rest clear.
    static std::shared_ptr<DxuiIconImage>  Scale (const RgbaImage & picture, int sizePx);

    //  A thumbnail's record as a line for the user.
    static std::wstring  DescribeRecord (const ThumbnailRecord & record, bool isUserSet);

    //  Whether a file of the size can be the image of a 5.25" disk.
    static bool  IsFloppySize (const std::wstring & extension, uint64_t bytes);

private:
    //  The image's thumbnail, decoded. False with none, once the image has
    //  been passed to the queue function.
    bool  Read (const std::wstring & path, RgbaImage & outPicture, ThumbnailRecord & outRecord, bool & outIsUserSet);

    struct Target
    {
        HWND  hwnd    = nullptr;
        UINT  message = 0;
    };

    static constexpr uint64_t  s_kMaxSectorBytes = 250000;
    static constexpr uint64_t  s_kMaxWozBytes    = 4 * 1024 * 1024;

    ThumbnailStore          & m_store;
    QueueFn                   m_queue;
    std::mutex                m_lock;
    std::vector<Target>       m_targets;
    std::set<std::wstring>    m_queued;      // each image passed to the queue function, by path, size and time
};
