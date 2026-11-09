#include "Pch.h"

#include "CassoExplorer/CassoExplorerAbout.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Printer/PngCodec.h"
#include "Ui/Dialogs/AttributionsText.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Version.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::GetBody
//
//  The heading lines sit beside the photograph, so they come first and end
//  with a blank line that clears it. Then the name as an equation of the
//  three icons, which needs no sentence to introduce it, standing off the
//  text above it rather than the rows it belongs with, a row for each icon,
//  and the links, grouped in pairs by blank lines the way Casso's About box
//  groups them. An icon left empty gives its place in the equation to its
//  word and leaves its row as text alone.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DialogTextRun> CassoExplorerAbout::GetBody()
{
    std::vector<DialogTextRun>  runs;
    std::wstring                repository = L"Casso on GitHub ";



    runs.push_back ({ L"Casso Explorer" });
    runs.push_back ({ L"Copyright (C) by Robert Elmer" });
    runs.push_back ({ L"" });
    runs.push_back ({ L"Version " VERSION_STRING });
    runs.push_back ({ L"Built " VERSION_BUILD_TIMESTAMP });
    runs.push_back ({ L"" });
    runs.push_back ({ L"An Apple II disk image explorer." });
    runs.push_back ({ L"" });

    repository += s_kchEmDash;
    repository += L" ";
    repository += s_kpszStar;
    repository += L" stars welcome";

    runs.push_back (MakeLink (repository,  kRepositoryUrl));
    runs.push_back (MakeLink (L"Log a bug", kBugReportUrl));
    runs.push_back ({ L"" });
    runs.push_back (MakeLink (L"MIT License", kLicenseUrl));

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::MakeSized
//
//  A copy of the picture to be shown at displayDp. An empty picture stays
//  empty, so the body still reads it as missing.
//
////////////////////////////////////////////////////////////////////////////////

DialogImage CassoExplorerAbout::MakeSized (const DialogImage & image, float displayDp)
{
    DialogImage  sized = image;



    if (!sized.rgba.empty())
    {
        sized.displayDp = displayDp;
    }

    return sized;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::MakeLeadingRun
//
////////////////////////////////////////////////////////////////////////////////

DialogTextRun CassoExplorerAbout::MakeLeadingRun (const DialogImage & picture, const wchar_t * text)
{
    DialogTextRun  run;



    run.text = text;

    if (!picture.rgba.empty())
    {
        run.leadingImage = MakeSized (picture, kRowPictureDp);
    }

    return run;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::MakeLink
//
////////////////////////////////////////////////////////////////////////////////

DialogTextRun CassoExplorerAbout::MakeLink (const std::wstring & text, const wchar_t * url)
{
    DialogTextRun  run;



    run.text         = text;
    run.isHyperlink  = true;
    run.hyperlinkUrl = url;

    return run;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::LoadPicture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerAbout::LoadPicture (HINSTANCE instance, int resourceId, float displayDp, DialogImage & outImage)
{
    HRESULT            hr       = S_OK;
    HRSRC              resource = FindResourceW (instance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
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
    outImage.displayDp = displayDp;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::Show
//
//  The pictures are optional: a build whose resources lack them still
//  says what the application is.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerAbout::Show (HWND owner, const IDxuiTheme * theme, HINSTANCE instance)
{
    static constexpr int                kWidthDip        = 440;
    static constexpr int                kChromeHeightDip = 108;
    static constexpr int                kMaxHeightDip    = 760;
    std::unique_ptr<DialogBodyContent>  content          = std::make_unique<DialogBodyContent>();
    MessageDialog                       dialog;
    DxuiWindow::CreateParams            params;
    DialogImage                         photo;
    HRESULT                             hr               = S_OK;
    int                                 result           = 0;



    content->SetRuns (GetBody());
    content->SetImagePlacement (DialogBodyContent::ImagePlacement::CenteredAbove);

    hr = LoadPicture (instance, IDR_CASSO_EXPLORER_CASSOWARY_PNG, kHeaderPictureDp, photo);

    if (SUCCEEDED (hr))
    {
        content->SetImage (photo);
    }

    params.title                    = L"About Casso Explorer";
    params.hInstance                = instance;
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, (std::min) (kChromeHeightDip + content->GetPreferredHeightDip(), kMaxHeightDip) };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    dialog.Configure (std::move (content),
                      { { L"Attributions", kAttributionsResult, false, false }, { L"OK", IDOK, true, true } },
                      IDOK);

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return;
    }

    dialog.SetTheme (theme);
    result = dialog.TranslateResult (dialog.ShowModalDialog (dialog.GetDefaultCommandId()));

    if (result == kAttributionsResult)
    {
        ShowAttributions (owner, theme, instance);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAbout::ShowAttributions
//
//  The same list Casso shows, from AttributionsText, in a dialog of the same
//  width as the About box it opens from.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerAbout::ShowAttributions (HWND owner, const IDxuiTheme * theme, HINSTANCE instance)
{
    static constexpr int                kWidthDip        = 440;
    static constexpr int                kChromeHeightDip = 108;
    static constexpr int                kMaxHeightDip    = 760;
    std::unique_ptr<DialogBodyContent>  content          = std::make_unique<DialogBodyContent>();
    MessageDialog                       dialog;
    DxuiWindow::CreateParams            params;
    HRESULT                             hr               = S_OK;
    int                                 result           = 0;



    content->SetRuns (AttributionsText::BuildBody());

    params.title                    = L"Attributions";
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
