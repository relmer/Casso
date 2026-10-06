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
    Adopt (m_header);
    Adopt (m_date);
    Adopt (m_scroll);
    Adopt (m_status);
    Adopt (m_pageLink);

    m_scroll.Adopt (m_notes);

    m_header.SetTextRole   (DxuiTextRole::Heading);
    m_header.SetFontWeight (DxuiFontWeight::Bold);
    m_header.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_date.SetTextRole  (DxuiTextRole::Muted);
    m_date.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

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
//  UpdateDialogContent::SetHeader
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetHeader (const std::wstring & header, const std::wstring & dateLine)
{
    m_header.SetText (header);
    m_date.SetText   (dateLine);
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
    m_status.SetText     (status);
    m_status.SetTextRole (isError ? DxuiTextRole::Error : DxuiTextRole::Body);
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
    RECT  notesPx = m_notesViewportPx;
    int   height  = 0;



    if (!m_isLaidOut)
    {
        return;
    }

    height           = m_notes.GetEstimatedHeightPx (m_scaler);
    m_placedHeightPx = m_notes.GetMeasuredHeightPx();

    notesPx.right  -= m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip);
    notesPx.bottom  = notesPx.top + height;

    m_scroll.SetLineStepPx (m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip * 2));
    m_scroll.PlaceChild    (m_notes, notesPx);
    m_scroll.Layout        (m_notesViewportPx, m_scaler);
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
    constexpr int  kLinkHeightDip = 22;



    int   line   = scaler.ToPx (kLineDip);
    int   gap    = scaler.ToPx (kGapDip);
    int   link   = scaler.ToPx (kLinkHeightDip);
    int   y      = boundsPx.top;
    int   bottom = boundsPx.bottom;



    SetBounds (boundsPx);
    m_scaler    = scaler;
    m_isLaidOut = true;

    m_header.Layout (RECT { boundsPx.left, y, boundsPx.right, y + line }, scaler);
    y += line;

    m_date.Layout (RECT { boundsPx.left, y, boundsPx.right, y + line }, scaler);
    y += line + gap;

    m_pageLink.Layout (RECT { boundsPx.left, bottom - link, boundsPx.right, bottom });
    m_pageLink.SetDpi (scaler.GetDpi());
    bottom -= link;

    m_status.Layout (RECT { boundsPx.left, bottom - line * kStatusLines, boundsPx.right, bottom }, scaler);
    bottom -= line * kStatusLines + gap;

    m_notesViewportPx = RECT { boundsPx.left, y, boundsPx.right, std::max (bottom, y) };

    LayoutNotes();
}
