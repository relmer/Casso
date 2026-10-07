#include "Pch.h"

#include "Ui/Dialogs/UpdateDialogContent.h"
#include "Core/TextEncoding.h"
#include "Update/UpdateDialogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::UpdateDialogContent
//
//  The widgets are members, adopted so the panel walks paint and route
//  input to them. The notes view is adopted by the scroll panel, which
//  moves and clips it.
//
////////////////////////////////////////////////////////////////////////////////

UpdateDialogContent::UpdateDialogContent()
{
    constexpr float  kOpenerFontDip = 18.0f;



    Adopt (m_opener);
    Adopt (m_header);
    Adopt (m_scroll);
    Adopt (m_status);
    Adopt (m_pageLink);

    m_scroll.Adopt (m_notes);

    m_opener.SetTextRole    (DxuiTextRole::Heading);
    m_opener.SetFontWeight  (DxuiFontWeight::Bold);
    m_opener.SetFontSizeDip (kOpenerFontDip);
    m_opener.SetTextAlign   (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_header.SetTextRole   (DxuiTextRole::Heading);
    m_header.SetFontWeight (DxuiFontWeight::Bold);
    m_header.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_status.SetTextRole  (DxuiTextRole::Body);
    m_status.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_pageLink.SetLabel   (UpdateDialogModel::kpszPageLink);
    m_pageLink.SetVariant (DxuiButton::Variant::Link);
    m_pageLink.SetOnClick ([this] ()
    {
        if (m_onOpenUrl && !m_pageUrl.empty())
        {
            m_onOpenUrl (m_pageUrl);
        }
    });

    m_notes.SetOnOpenLink ([this] (const std::string & url)
    {
        if (m_onOpenUrl)
        {
            m_onOpenUrl (TextEncoding::Utf8ToWide (url));
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetOpener
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetOpener (const std::wstring & opener)
{
    m_opener.SetText (opener);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetHeader
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetHeader (const std::wstring & header)
{
    m_header.SetText (header);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetNotesLines
//
//  New notes start at the top, with the estimated height until the next
//  paint measures them.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetNotesLines (std::vector<FormattedLine> lines)
{
    m_notes.SetLines (std::move (lines));
    m_scroll.SetScrollPosPx (0);
    m_placedHeightPx = 0;

    LayoutNotes();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetNotesMessage
//
//  A one-line notice in place of the notes, while they load or when they
//  could not be read.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetNotesMessage (const std::wstring & message)
{
    std::vector<FormattedLine>  lines (1);
    FormattedRun                run;



    run.text = TextEncoding::WideToUtf8 (message);
    lines[0].runs.push_back (run);

    SetNotesLines (std::move (lines));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetStatus
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetStatus (const std::wstring & status, bool isError)
{
    bool  wasShown = m_hasStatus;



    m_status.SetText     (status);
    m_status.SetTextRole (isError ? DxuiTextRole::Error : DxuiTextRole::Body);
    m_hasStatus = !status.empty();

    // The status rows are reserved only while there is a status, so the
    // notes reclaim them otherwise; a change either way moves the layout.
    if (m_isLaidOut && wasShown != m_hasStatus)
    {
        Layout (GetBounds(), m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetNotesImage
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetNotesImage (const std::string & src, std::shared_ptr<const NotesImage> image)
{
    m_notes.SetImage (src, std::move (image));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetPageUrl
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetPageUrl (const std::wstring & url)
{
    m_pageUrl = url;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SyncNotesHeight
//
//  Lays the notes out again when painting measured a height other than
//  the one they were placed with. True when it did, so the caller repaints.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateDialogContent::SyncNotesHeight()
{
    int   measured  = m_notes.GetMeasuredHeightPx();
    bool  isChanged = m_isLaidOut && measured > 0 && measured != m_placedHeightPx;



    if (isChanged)
    {
        LayoutNotes();
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::LayoutNotes
//
//  The notes view is as tall as its content and as wide as the viewport
//  less the scrollbar strip, which the scroll panel needs left free.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::LayoutNotes()
{
    RECT  notesPx   = m_notesViewportPx;
    int   height    = 0;
    int   oldPos    = 0;
    int   oldHeight = 0;



    if (!m_isLaidOut)
    {
        return;
    }

    oldPos           = m_scroll.GetScrollPosPx();
    oldHeight        = m_scroll.GetContentHeightPx();
    height           = m_notes.GetEstimatedHeightPx (m_scaler);
    m_placedHeightPx = m_notes.GetMeasuredHeightPx();

    notesPx.right  -= m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip);
    notesPx.bottom  = notesPx.top + height;

    m_scroll.SetLineStepPx (m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip * 2));
    m_scroll.PlaceChild    (m_notes, notesPx);
    m_scroll.Layout        (m_notesViewportPx, m_scaler);

    // A resize rewraps the notes to a new height; keep the reader the same
    // fraction of the way down rather than snapping back to the top.
    m_scroll.SetScrollPosPx (ReleaseNotesLayout::ScaleScrollPos (oldPos, oldHeight, m_scroll.GetContentHeightPx()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::Layout
//
//  Fixed rows at the top and bottom; the notes take what is left.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    constexpr int  kLineDip       = 20;
    constexpr int  kGapDip        = 8;
    constexpr int  kStatusLines   = 2;
    constexpr int  kHeaderLines   = 3;
    constexpr int  kOpenerDip     = 30;
    constexpr int  kLinkHeightDip = 22;



    int  line    = scaler.ToPx (kLineDip);
    int  opener  = scaler.ToPx (kOpenerDip);
    int  gap     = scaler.ToPx (kGapDip);
    int  link    = scaler.ToPx (kLinkHeightDip);
    int  y       = boundsPx.top;
    int  bottom  = boundsPx.bottom;
    int  statusH = 0;



    SetBounds (boundsPx);
    m_scaler    = scaler;
    m_isLaidOut = true;

    m_opener.Layout (RECT { boundsPx.left, y, boundsPx.right, y + opener }, scaler);
    y += opener;

    // Room for the versions sentence wrapped once, plus an age remark below it.
    m_header.Layout (RECT { boundsPx.left, y, boundsPx.right, y + line * kHeaderLines }, scaler);
    y += line * kHeaderLines + gap;

    m_pageLink.Layout (RECT { boundsPx.left, bottom - link, boundsPx.right, bottom });
    m_pageLink.SetDpi (scaler.GetDpi());
    bottom -= link;

    statusH = m_hasStatus ? line * kStatusLines : 0;
    m_status.Layout (RECT { boundsPx.left, bottom - statusH, boundsPx.right, bottom }, scaler);
    m_status.SetVisible (m_hasStatus);
    bottom -= statusH + gap;

    m_notesViewportPx = RECT { boundsPx.left, y, boundsPx.right, std::max (bottom, y) };

    LayoutNotes();
}
