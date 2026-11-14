#pragma once

#include "Pch.h"
#include "Seams/IDiskPictures.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDiskPictures
//
//  Every disk image has one picture of one color and one result line, and
//  each path and size passed to Load is added to a list. Load is called on
//  an icon loader's thread, so the list is under a lock.
//
////////////////////////////////////////////////////////////////////////////////

class FakeDiskPictures : public IDiskPictures
{
public:
    static constexpr uint32_t  kColor  = 0xFF336699;
    static constexpr int       kWidth  = 560;
    static constexpr int       kHeight = 384;

    std::wstring  resultLine = L"Boots on the Apple //e.";



    std::shared_ptr<DxuiIconImage>  Load (const std::wstring & path, int sizePx) override
    {
        std::shared_ptr<DxuiIconImage>  image = std::make_shared<DxuiIconImage>();
        std::lock_guard                 lock (m_lock);



        image->width  = sizePx;
        image->height = sizePx;
        image->bgraPremul.assign ((size_t) sizePx * sizePx, kColor);

        m_loaded.push_back ({ path, sizePx });

        return image;
    }



    bool  Find (const std::wstring & path, std::shared_ptr<const DxuiIconImage> & outPicture, std::wstring & outResult) override
    {
        std::shared_ptr<DxuiIconImage>  image = std::make_shared<DxuiIconImage>();



        (void) path;

        image->width  = kWidth;
        image->height = kHeight;
        image->bgraPremul.assign ((size_t) kWidth * kHeight, kColor);

        outPicture = image;
        outResult  = resultLine;

        return true;
    }



    std::vector<std::pair<std::wstring, int>>  GetLoaded()
    {
        std::lock_guard  lock (m_lock);



        return m_loaded;
    }



    void                      AddTarget     (HWND, UINT) override                      {}
    void                      RemoveTarget  (HWND) override                            {}
    bool                      IsUserSet     (const std::wstring &) override            { return false; }
    HRESULT                   RemoveUserSet (const std::wstring &) override            { return S_OK; }
    std::vector<std::string>  GetKeys       (const std::vector<std::wstring> &) override { return {}; }
    void                      RemoveDeleted (const std::vector<std::string> &) override  {}
    int                       CountUserSet  () override                                { return 0; }
    HRESULT                   Clear         (bool) override                            { return S_OK; }

private:
    std::mutex                                 m_lock;
    std::vector<std::pair<std::wstring, int>>  m_loaded;
};
