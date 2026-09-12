#include "Pch.h"

#include "Cassque/CassqueAbout.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Printer/PngCodec.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Version.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::GetBody
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

std::vector<DialogTextRun> CassqueAbout::GetBody (const Pictures & pictures)
{
    std::vector<DialogTextRun>  runs;
    DialogTextRun               equation;
    std::wstring                repository = L"Casso on GitHub ";



    runs.push_back ({ L"Cassque" });
    runs.push_back ({ L"Copyright (C) by Robert Elmer" });
    runs.push_back ({ L"" });
    runs.push_back ({ L"Version " VERSION_STRING });
    runs.push_back ({ L"Built " VERSION_BUILD_TIMESTAMP });
    runs.push_back ({ L"" });
    runs.push_back ({ L"An Apple II disk image explorer." });
    runs.push_back ({ L"" });
    runs.push_back ({ L"" });

    equation.strip = { { MakeSized (pictures.cask,    kEquationPictureDp), L"Cask"    },
                       { {},                                               L"+"       },
                       { MakeSized (pictures.casso,   kEquationPictureDp), L"Casso"   },
                       { {},                                               L"="       },
                       { MakeSized (pictures.cassque, kEquationPictureDp), L"Cassque" } };
    runs.push_back (equation);

    runs.push_back (MakeLeadingRun (pictures.cask,    L"Cask, a container that stores things, and a homophone of casque, the keratin-covered crest atop a cassowary's head"));
    runs.push_back (MakeLeadingRun (pictures.casso,   L"Casso, a spiffy Apple II emulator"));
    runs.push_back (MakeLeadingRun (pictures.cassque, L"Thus, Cassque, Casso's Apple II disk image explorer"));
    runs.push_back ({ L"" });

    repository += s_kchEmDash;
    repository += L" ";
    repository += s_kpszStar;
    repository += L" stars welcome";

    runs.push_back (MakeLink (repository,  kRepositoryUrl));
    runs.push_back (MakeLink (L"Log a bug", kBugReportUrl));
    runs.push_back ({ L"" });
    runs.push_back (MakeLink (L"MIT License", kLicenseUrl));
    runs.push_back (MakeLink (L"Cassowary photo by Mr. Smiley / BunyipCo, CC BY-NC-SA 3.0", kPhotoCreditUrl));

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::MakeSized
//
//  A copy of the picture to be shown at displayDp. An empty picture stays
//  empty, so the body still reads it as missing.
//
////////////////////////////////////////////////////////////////////////////////

DialogImage CassqueAbout::MakeSized (const DialogImage & image, float displayDp)
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
//  CassqueAbout::MakeLeadingRun
//
////////////////////////////////////////////////////////////////////////////////

DialogTextRun CassqueAbout::MakeLeadingRun (const DialogImage & picture, const wchar_t * text)
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
//  CassqueAbout::MakeLink
//
////////////////////////////////////////////////////////////////////////////////

DialogTextRun CassqueAbout::MakeLink (const std::wstring & text, const wchar_t * url)
{
    DialogTextRun  run;



    run.text         = text;
    run.isHyperlink  = true;
    run.hyperlinkUrl = url;

    return run;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::LoadPicture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueAbout::LoadPicture (HINSTANCE instance, int resourceId, float displayDp, DialogImage & outImage)
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
//  CassqueAbout::LoadPictures
//
//  An icon that fails to load stays empty, and the body shows its word.
//
////////////////////////////////////////////////////////////////////////////////

CassqueAbout::Pictures CassqueAbout::LoadPictures (HINSTANCE instance)
{
    Pictures  pictures;
    HRESULT   hr       = S_OK;



    hr = LoadPicture (instance, IDR_CASSQUE_CASK_PNG, kRowPictureDp, pictures.cask);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = LoadPicture (instance, IDR_CASSQUE_CASSO_PNG, kRowPictureDp, pictures.casso);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = LoadPicture (instance, IDR_CASSQUE_PICTURE_PNG, kRowPictureDp, pictures.cassque);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return pictures;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout::Show
//
//  The pictures are optional: a build without them still says what the name
//  means.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueAbout::Show (HWND owner, const IDxuiTheme * theme, HINSTANCE instance)
{
    static constexpr int                kWidthDip        = 480;
    static constexpr int                kChromeHeightDip = 108;
    static constexpr int                kMaxHeightDip    = 760;
    std::unique_ptr<DialogBodyContent>  content          = std::make_unique<DialogBodyContent>();
    MessageDialog                       dialog;
    DxuiWindow::CreateParams            params;
    DialogImage                         photo;
    HRESULT                             hr               = S_OK;
    int                                 result           = 0;



    content->SetRuns (GetBody (LoadPictures (instance)));
    content->SetImagePlacement (DialogBodyContent::ImagePlacement::TrailingBeside);

    hr = LoadPicture (instance, IDR_CASSQUE_CASSOWARY_PNG, kHeaderPictureDp, photo);

    if (SUCCEEDED (hr))
    {
        content->SetImage (photo);
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
