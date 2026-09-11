#include "Pch.h"

#include "Cassque/CassqueAbout.h"
#include "Devices/Printer/PngCodec.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Version.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::GetBody
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DialogTextRun> CassqueAbout::GetBody()
{
    std::vector<DialogTextRun>  runs;
    DialogTextRun               credit;



    runs.push_back ({ L"Cassque " VERSION_STRING });
    runs.push_back ({ L"" });
    runs.push_back ({ L"Browses Apple II disk images: their catalogs, their files, and what the files hold." });
    runs.push_back ({ L"" });
    runs.push_back ({ L"Where the name comes from:" });
    runs.push_back ({ L"    cask, a container, the barrel that holds the disks" });
    runs.push_back ({ L"  + casque, the tall helmet-like crest on a cassowary's head" });
    runs.push_back ({ L"  = Cassque, the browser, and the cassowary Casso is named for" });
    runs.push_back ({ L"" });

    credit.text         = L"Cassowary photo by Mr. Smiley / BunyipCo, CC BY-NC-SA 3.0";
    credit.isHyperlink  = true;
    credit.hyperlinkUrl = kPhotoCreditUrl;
    runs.push_back (credit);

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::LoadPicture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueAbout::LoadPicture (HINSTANCE instance, DialogImage & outImage)
{
    HRESULT            hr       = S_OK;
    HRSRC              resource = FindResourceW (instance, MAKEINTRESOURCEW (IDR_CASSQUE_CASSOWARY_PNG), RT_RCDATA);
    HGLOBAL            loaded   = nullptr;
    const Byte       * data     = nullptr;
    DWORD              size     = 0;
    std::vector<Byte>  png;
    RgbaImage          image;



    CWR (resource != nullptr);

    loaded = LoadResource (instance, resource);
    CWR (loaded != nullptr);

    data = (const Byte *) LockResource (loaded);
    size = SizeofResource (instance, resource);
    CBREx (data != nullptr && size > 0, HRESULT_FROM_WIN32 (ERROR_RESOURCE_DATA_NOT_FOUND));

    png.assign (data, data + size);

    hr = PngCodec::DecodeRgba (png, image);
    CHR (hr);

    outImage.rgba      = std::move (image.rgba);
    outImage.width     = image.width;
    outImage.height    = image.height;
    outImage.displayDp = kPictureDp;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::Show
//
//  The picture is optional: a build without it still says what the name
//  means.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueAbout::Show (HWND owner, const IDxuiTheme * theme, HINSTANCE instance)
{
    static constexpr int                kWidthDip        = 460;
    static constexpr int                kChromeHeightDip = 108;
    static constexpr int                kMaxHeightDip    = 640;
    std::unique_ptr<DialogBodyContent>  content          = std::make_unique<DialogBodyContent>();
    MessageDialog                       dialog;
    DxuiWindow::CreateParams            params;
    DialogImage                         picture;
    HRESULT                             hr               = S_OK;
    int                                 result           = 0;



    content->SetRuns (GetBody());

    hr = LoadPicture (instance, picture);

    if (SUCCEEDED (hr))
    {
        content->SetImage (picture);
    }

    params.title                    = L"About Cassque";
    params.hInstance                = instance;
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, (std::min) (kChromeHeightDip + content->GetPreferredHeightDip(), kMaxHeightDip) };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    dialog.Configure (std::move (content), { { L"OK", IDOK, true, true } }, IDOK);

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return;
    }

    dialog.SetTheme (theme);
    result = dialog.ShowModalDialog (dialog.GetDefaultCommandId());
    IGNORE_RETURN_VALUE (result, IDOK);
}
