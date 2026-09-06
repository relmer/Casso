#include "Pch.h"

#include "ScreenshotsPage.h"




// Layout metrics (DIP).
static constexpr int    s_kRowHeightDp     = 28;
static constexpr int    s_kLabelWidthDp    = 130;
static constexpr int    s_kCheckWidthDp    = 220;
static constexpr int    s_kChildIndentDp   = 18;    // one nesting step (matches DxuiTreeView)
static constexpr int    s_kSectionGapDp    = 14;
static constexpr int    s_kPagePadDp       = 16;

// A described option: the label line, its description under it, and a gap
// before the next option. The gap is part of the option's own box so the
// group stays one contiguous hit region -- a described option must select
// from anywhere in it, description included.
static constexpr int    s_kOptionLabelDp   = 20;
static constexpr int    s_kOptionDescDp    = 20;
static constexpr int    s_kOptionGapDp     = 14;

// Between the last option and the save row. The radios are a group; the
// checkbox below them is a separate question and wants to read that way.
static constexpr int    s_kAfterRadiosDp   = 22;

static constexpr int    s_kBrowseWidthDp   = 100;
static constexpr int    s_kLinkWidthDp     = 260;
static constexpr int    s_kLinkGapDp       = 10;
static constexpr size_t s_kFolderMaxChars  = 42;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ScreenshotsPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::CaptureModeToIndex
//
//  Stored token -> radio index, in the radio order: scene / crt / raw.
//
//  Spelled out rather than cast from the enum. The enum's declaration order
//  and the radio's display order are two different things that happen to
//  agree today, and a cast would quietly turn a change to either one into a
//  wrong setting rather than a compile error.
//
////////////////////////////////////////////////////////////////////////////////

int ScreenshotsPage::CaptureModeToIndex (const std::string & token)
{
    int   index = 0;



    if (token == "crt")
    {
        index = 1;
    }
    else if (token == "raw")
    {
        index = 2;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::IndexToCaptureMode
//
//  The inverse. An index outside the set resolves to the default rather than
//  writing a token nothing can parse.
//
////////////////////////////////////////////////////////////////////////////////

const char * ScreenshotsPage::IndexToCaptureMode (int index)
{
    const char *   token = "scene";



    if (index == 1)
    {
        token = "crt";
    }
    else if (index == 2)
    {
        token = "raw";
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::FolderForDisplay
//
//  What the folder row says.
//
//  An unset preference means "the default", and the row shows the default's
//  real path rather than an empty field -- blank reads as broken when the
//  feature is in fact working, and the user cannot tell where their files are
//  going without leaving the page.
//
//  A path too long for the row keeps its LAST components. The tail is what
//  distinguishes one configured folder from another; the head is what every
//  path on the machine has in common, so it is the half worth losing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ScreenshotsPage::FolderForDisplay (const std::string &  configured,
                                                const std::wstring & defaultFolder,
                                                size_t               maxChars)
{
    std::wstring   path;
    size_t         cut  = std::wstring::npos;



    if (configured.empty())
    {
        path = defaultFolder;
    }
    else
    {
        path.assign (configured.begin(), configured.end());
    }

    if (path.length() <= maxChars || maxChars < 8)
    {
        return path;
    }

    //  Walk back from the end to a separator, so the ellipsis lands on a
    //  component boundary rather than mid-name.
    cut = path.rfind (L'\\', path.length() - 1);

    if (cut != std::wstring::npos && cut > 0)
    {
        cut = path.rfind (L'\\', cut - 1);
    }

    if (cut == std::wstring::npos || (path.length() - cut) > maxChars)
    {
        return L"..." + path.substr (path.length() - (maxChars - 3));
    }

    return L"..." + path.substr (cut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::ScreenshotsPage
//
//  Registers each member widget in the page's child tree (non-owning Adopt);
//  Layout positions them and Rebuild wires their callbacks.
//
////////////////////////////////////////////////////////////////////////////////

ScreenshotsPage::ScreenshotsPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_captureLabel);
    Adopt (m_captureMode);
    Adopt (m_saveFile);
    Adopt (m_folderLabel);
    Adopt (m_folderLink);
    Adopt (m_browseFolder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::SetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void ScreenshotsPage::SetPrefs (GlobalUserPrefs * prefs)
{
    m_prefs = prefs;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::SetPopupHost
//
//  Nothing on this page opens a popup menu; the hook exists so the sheet can
//  treat every page alike.
//
////////////////////////////////////////////////////////////////////////////////

void ScreenshotsPage::SetPopupHost (DxuiHwndSource * host)
{
    UNREFERENCED_PARAMETER (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::RefreshFolderText
//
//  Puts the current destination on the folder link.
//
//  Called from BOTH Layout and Rebuild, which is the point: the sheet supplies
//  the default folder and the prefs after the page has already laid out once,
//  so a Layout-only assignment leaves the row blank until something else
//  provokes a re-layout -- and a blank row reads as a broken setting rather
//  than an unset one.
//
////////////////////////////////////////////////////////////////////////////////

void ScreenshotsPage::RefreshFolderText()
{
    m_folderLink.SetLabel (FolderForDisplay (m_prefs != nullptr ? m_prefs->screenshotFolder : string(),
                                             m_defaultFolder,
                                             s_kFolderMaxChars));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::Layout
//
//  No section heading: the tab is called Screenshots and repeating it at the
//  top of its own page says nothing the user has not just read.
//
////////////////////////////////////////////////////////////////////////////////

void ScreenshotsPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi         = scaler.GetDpi();
    int  pad         = scaler.ToPx (s_kPagePadDp);
    int  rowHeight   = scaler.ToPx (s_kRowHeightDp);
    int  labelWidth  = scaler.ToPx (s_kLabelWidthDp);
    int  checkWidth  = scaler.ToPx (s_kCheckWidthDp);
    int  childIndent = scaler.ToPx (s_kChildIndentDp);
    int  sectionGap  = scaler.ToPx (s_kSectionGapDp);
    int  x           = rect.left + pad;
    int  y           = rect.top  + pad;
    int  controlsX   = x + labelWidth;



    m_captureLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_captureLabel.SetText (L"Capture:");

    {
        int   labelH  = scaler.ToPx (s_kOptionLabelDp);
        int   descH   = scaler.ToPx (s_kOptionDescDp);
        int   gapH    = scaler.ToPx (s_kOptionGapDp);
        int   optionH = labelH + descH + gapH;
        int   optionW = (rect.right - rect.left) - (controlsX - rect.left) - pad;
        int   optionY = y;

        std::vector<DxuiRadioOption>  options;

        auto  addOption = [&] (const wchar_t * label, const wchar_t * description)
        {
            DxuiRadioOption   opt;

            opt.rect        = MakeRect (controlsX, optionY, optionW, optionH);
            opt.label       = label;
            opt.description = description;
            options.push_back (opt);
            optionY += optionH;
        };

        addOption (L"Scene",   L"The desk as it looks: monitor, glass and drives.");
        addOption (L"Picture", L"The screen with its CRT effects, nothing around it.");
        addOption (L"Raw",     L"The unprocessed screen, always 560x384.");

        m_captureMode.SetOptions (options);
        m_captureMode.SetBounds  (MakeRect (controlsX, y, optionW, optionH * 3));

        //  The trailing gap belongs to the last option's box, so back it off
        //  before adding the separation the save row wants.
        y = optionY - gapH + scaler.ToPx (s_kAfterRadiosDp);
    }

    m_saveFile.SetRect  (MakeRect (x, y, labelWidth + checkWidth, rowHeight));
    m_saveFile.SetLabel (L"Save a file as well as copying");
    y += rowHeight + sectionGap;

    {
        int   linkW   = scaler.ToPx (s_kLinkWidthDp);
        int   browseW = scaler.ToPx (s_kBrowseWidthDp);
        int   gap     = scaler.ToPx (s_kLinkGapDp);

        m_folderLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
        m_folderLabel.SetText (L"Folder:");

        m_folderLink.SetVariant (DxuiButton::Variant::Link);
        m_folderLink.Layout     (MakeRect (controlsX, y, linkW, rowHeight));
        RefreshFolderText();

        m_browseFolder.SetLabel (L"Browse...");
        m_browseFolder.Layout   (MakeRect (controlsX + linkW + gap, y, browseW, rowHeight));

        y += rowHeight + sectionGap;
    }

    m_captureLabel.SetDpi (dpi);
    m_captureMode.SetDpi  (dpi);
    m_saveFile.SetDpi     (dpi);
    m_folderLabel.SetDpi  (dpi);
    m_folderLink.SetDpi   (dpi);
    m_browseFolder.SetDpi (dpi);

    DxuiPanel::SetBounds (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::ApplyEnabledState
//
//  With saving off there is no file to have a folder for, so the folder row
//  dims rather than sitting there inviting a change that does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ScreenshotsPage::ApplyEnabledState()
{
    bool          savingOn = (m_prefs != nullptr) && m_prefs->screenshotSaveFile;
    DxuiTextRole  role     = savingOn ? DxuiTextRole::Body : DxuiTextRole::Disabled;



    m_folderLink.SetEnabled   (savingOn);
    m_browseFolder.SetEnabled (savingOn);
    m_folderLabel.SetTextRole (role);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage::Rebuild
//
//  Seeds every widget from the prefs and wires the change callbacks.
//
////////////////////////////////////////////////////////////////////////////////

void ScreenshotsPage::Rebuild()
{
    GlobalUserPrefs *  prefs = m_prefs;



    if (prefs == nullptr)
    {
        return;
    }

    m_captureMode.SetSelected (CaptureModeToIndex (prefs->screenshotMode));
    m_saveFile.SetChecked     (prefs->screenshotSaveFile);
    RefreshFolderText();
    ApplyEnabledState();

    m_captureMode.SetOnChange ([this, prefs] (int idx)
    {
        prefs->screenshotMode = IndexToCaptureMode (idx);
        MarkDirty();
    });

    m_saveFile.SetOnChange ([this, prefs] (bool checked)
    {
        prefs->screenshotSaveFile = checked;
        ApplyEnabledState();
        MarkDirty();
    });

    m_folderLink.SetOnClick ([this] ()
    {
        if (m_onOpenFolder)
        {
            m_onOpenFolder();
        }
    });

    m_browseFolder.SetOnClick ([this] ()
    {
        if (m_onBrowseFolder)
        {
            m_onBrowseFolder();
        }
    });
}
