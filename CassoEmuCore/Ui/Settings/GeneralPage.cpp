#include "Pch.h"

#include "GeneralPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT GeneralPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::WireToggle
//
//  Forwards a checkbox's changes to the page's callback member. The member
//  is read at the click, so a callback the sheet installs later still runs.
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::WireToggle (DxuiCheckbox & checkbox, const ToggleFn & fn)
{
    checkbox.SetSingleLineLabel (true);
    checkbox.SetOnChange ([&fn] (bool checked)
    {
        if (fn)
        {
            fn (checked);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::GeneralPage
//
//  Registers every control in the page's child tree (non-owning Adopt) and
//  forwards each one's changes to the callback the sheet installs.
//
////////////////////////////////////////////////////////////////////////////////

GeneralPage::GeneralPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_updatesHeading);
    Adopt (m_autoUpdateCheckbox);
    Adopt (m_lastCheckedLabel);
    Adopt (m_checkNowButton);
    Adopt (m_skippedLabel);
    Adopt (m_stopSkipButton);
    Adopt (m_downloadsHeading);
    Adopt (m_audioOfferCheckbox);
    Adopt (m_romOfferCheckbox);
    Adopt (m_folderHeading);
    Adopt (m_folderLink);

    // Each group gets a heading of its own over its rows, as on the Storage page.
    for (DxuiLabel * heading : { &m_updatesHeading, &m_downloadsHeading, &m_folderHeading })
    {
        heading->SetTextRole   (DxuiTextRole::Heading);
        heading->SetFontWeight (DxuiFontWeight::SemiBold);
    }

    m_updatesHeading.SetText   (L"Updates");
    m_downloadsHeading.SetText (L"Offer downloads at startup");
    m_folderHeading.SetText    (L"Settings folder");

    m_autoUpdateCheckbox.SetLabel (L"Check for updates automatically");
    m_audioOfferCheckbox.SetLabel (L"Disk drive sounds");
    m_romOfferCheckbox.SetLabel   (L"Updated ROMs");

    WireToggle (m_autoUpdateCheckbox, m_onAutoUpdateToggled);
    WireToggle (m_audioOfferCheckbox, m_onAudioOfferToggled);
    WireToggle (m_romOfferCheckbox,   m_onRomOfferToggled);

    m_lastCheckedLabel.SetTextRole (DxuiTextRole::Muted);
    m_skippedLabel.SetTextRole     (DxuiTextRole::Muted);
    m_lastCheckedLabel.SetText     (L"Never checked.");
    m_skippedLabel.SetVisible      (false);
    m_stopSkipButton.SetVisible    (false);

    m_checkNowButton.SetLabel (L"Check now");
    m_stopSkipButton.SetLabel (L"Cancel skip");

    // The path is long and its two ends identify it: the drive and user at
    // the front, the Casso folder at the back.
    m_folderLink.SetVariant (DxuiButton::Variant::Link);
    m_folderLink.SetElide   (DxuiElide::Middle);

    m_checkNowButton.SetOnClick ([this] { if (m_onCheckNow)     { m_onCheckNow();     } });
    m_stopSkipButton.SetOnClick ([this] { if (m_onStopSkipping) { m_onStopSkipping(); } });
    m_folderLink.SetOnClick     ([this] { if (m_onOpenFolder)   { m_onOpenFolder();   } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::SetLastCheckedText
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::SetLastCheckedText (const std::wstring & text)
{
    m_lastCheckedLabel.SetText (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::SetFolderPath
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::SetFolderPath (const std::wstring & path)
{
    m_folderLink.SetLabel (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::SetSkippedText
//
//  Shows or hides the skipped-release row with its button, and lays the page
//  out again so the groups below move to fit.
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::SetSkippedText (const std::wstring & text)
{
    bool  isShown = !text.empty();



    m_skippedLabel.SetText      (text);
    m_skippedLabel.SetVisible   (isShown);
    m_stopSkipButton.SetVisible (isShown);

    if (m_hasLayout)
    {
        Layout (m_lastRect, m_lastScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::Layout
//
//  Three headed groups down the left, inside the page pad. The status lines
//  indent under the update checkbox's label with their buttons in one column
//  beside them; the skipped row takes space only while it is shown. Rows are
//  clamped to the page's inner width so a narrow window cannot push them
//  past the right edge.
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi        = scaler.GetDpi();
    int   pad        = scaler.ToPx (kPagePadDp);
    int   rowHeight  = scaler.ToPx (kRowHeightDp);
    int   rowStep    = rowHeight + scaler.ToPx (kRowGapDp);
    int   sectionGap = scaler.ToPx (kSectionGapDp);
    int   indent     = scaler.ToPx (kTextIndentDp);
    int   x          = rect.left + pad;
    int   y          = rect.top  + pad;
    int   innerW     = std::max (0, (int) (rect.right - rect.left) - pad * 2);
    int   checkW     = std::min (scaler.ToPx (kCheckWidthDp), innerW);
    int   statusW    = scaler.ToPx (kStatusWidthDp);
    int   buttonW    = scaler.ToPx (kButtonWidthDp);
    int   buttonX    = x + indent + statusW;



    m_updatesHeading.SetRect (MakeRect (x, y, innerW, rowHeight));
    y += rowStep;

    m_autoUpdateCheckbox.Layout (MakeRect (x, y, checkW, rowHeight), scaler);
    y += rowStep;

    m_lastCheckedLabel.SetRect (MakeRect (x + indent, y, statusW, rowHeight));
    m_checkNowButton.Layout    (MakeRect (buttonX, y, buttonW, rowHeight));
    y += rowStep;

    m_skippedLabel.SetRect  (MakeRect (x + indent, y, statusW, rowHeight));
    m_stopSkipButton.Layout (MakeRect (buttonX, y, buttonW, rowHeight));

    if (m_skippedLabel.IsVisible())
    {
        y += rowStep;
    }

    y += sectionGap;

    m_downloadsHeading.SetRect (MakeRect (x, y, innerW, rowHeight));
    y += rowStep;

    m_audioOfferCheckbox.Layout (MakeRect (x, y, checkW, rowHeight), scaler);
    y += rowStep;

    m_romOfferCheckbox.Layout (MakeRect (x, y, checkW, rowHeight), scaler);
    y += rowStep + sectionGap;

    m_folderHeading.SetRect (MakeRect (x, y, innerW, rowHeight));
    y += rowStep;

    // Paint narrows the link to the path's measured width; until then it may
    // take the page's inner width.
    m_folderMaxWidthPx = innerW;
    m_folderLink.Layout (MakeRect (x, y, innerW, rowHeight));

    m_updatesHeading.SetDpi     (dpi);
    m_autoUpdateCheckbox.SetDpi (dpi);
    m_lastCheckedLabel.SetDpi   (dpi);
    m_checkNowButton.SetDpi     (dpi);
    m_skippedLabel.SetDpi       (dpi);
    m_stopSkipButton.SetDpi     (dpi);
    m_downloadsHeading.SetDpi   (dpi);
    m_audioOfferCheckbox.SetDpi (dpi);
    m_romOfferCheckbox.SetDpi   (dpi);
    m_folderHeading.SetDpi      (dpi);
    m_folderLink.SetDpi         (dpi);

    DxuiPanel::SetBounds (rect);

    m_hasLayout  = true;
    m_lastRect   = rect;
    m_lastScaler = scaler;

    // Every control is a fixed height, so the lowest visible one is where the
    // content ends.
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage::Paint
//
//  Narrows the folder link to its path's width, measured in the face and
//  size the link draws in, so only the text itself is the hit target. A path
//  wider than the page keeps the page's inner width, and the link elides it
//  in the middle. Painting is the first point where a text renderer is at
//  hand to measure with.
//
////////////////////////////////////////////////////////////////////////////////

void GeneralPage::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT       hr       = S_OK;
    std::wstring  label    = m_folderLink.GetAccessibleName();
    RECT          bounds   = m_folderLink.GetBounds();
    float         fontPx   = m_lastScaler.ToPxf (kLinkFontDp);
    float         widthPx  = 0.0f;
    float         heightPx = 0.0f;
    int           linkW    = 0;



    if (m_hasLayout)
    {
        hr = text.MeasureString (label.c_str(), fontPx, DxuiTheme::GetUiFace(), widthPx, heightPx);
        IGNORE_RETURN_VALUE (hr, S_OK);

        // A failed or empty measurement keeps the full width rather than
        // shrinking the link to nothing clickable.
        linkW        = widthPx > 0.0f ? std::min ((int) std::ceil (widthPx), m_folderMaxWidthPx) : m_folderMaxWidthPx;
        bounds.right = bounds.left + linkW;
        m_folderLink.Layout (bounds);
    }

    DxuiPanel::Paint (painter, text, theme);
}
