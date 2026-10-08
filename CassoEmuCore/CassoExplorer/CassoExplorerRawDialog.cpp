#include "Pch.h"

#include "CassoExplorer/CassoExplorerRawDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::TryParse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerRawChoices::TryParse (const std::wstring & text, int & outValue)
{
    size_t  first = text.find_first_not_of (L' ');
    size_t  last  = text.find_last_not_of (L' ');
    long    value = 0;



    if (first == std::wstring::npos || last - first + 1 > 6)
    {
        return false;
    }

    for (size_t i = first; i <= last; i++)
    {
        if (text[i] < L'0' || text[i] > L'9')
        {
            return false;
        }

        value = value * 10 + (text[i] - L'0');
    }

    outValue = (int) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::ValidateTrack
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerRawChoices::ValidateTrack (const std::wstring & text)
{
    int  value = 0;



    return (TryParse (text, value) && value < kTracks) ? std::wstring() : L"Enter a track from 0 to 34.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::ValidateSector
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerRawChoices::ValidateSector (const std::wstring & text)
{
    int  value = 0;



    return (TryParse (text, value) && value < kSectorsPerTrack) ? std::wstring() : L"Enter a sector from 0 to 15.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::ValidateBlock
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerRawChoices::ValidateBlock (const std::wstring & text)
{
    int  value = 0;



    return (TryParse (text, value) && value < kBlocks) ? std::wstring() : L"Enter a block from 0 to 279.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::ValidateCount
//
//  With no valid start there is nothing to measure against, and the start's
//  own message says what is wrong, so the count says nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerRawChoices::ValidateCount (const std::wstring & text, int available)
{
    int  value = 0;



    if (available <= 0)
    {
        return std::wstring();
    }

    if (TryParse (text, value) && value >= 1 && value <= available)
    {
        return std::wstring();
    }

    return L"Enter a count from 1 to " + std::to_wstring (available) + L", which reaches the end of the disk.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::GetSectorsFrom
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerRawChoices::GetSectorsFrom (int track, int sector)
{
    bool  valid = track >= 0 && track < kTracks && sector >= 0 && sector < kSectorsPerTrack;



    return valid ? kTracks * kSectorsPerTrack - (track * kSectorsPerTrack + sector) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices::GetBlocksFrom
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerRawChoices::GetBlocksFrom (int block)
{
    return (block >= 0 && block < kBlocks) ? kBlocks - block : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerRawPanel::Init (std::vector<Row> rows, DxuiLabel * note)
{
    m_rows = std::move (rows);
    m_note = note;

    for (const Row & row : m_rows)
    {
        Adopt (*row.label);
        Adopt (*row.field);

        if (row.error != nullptr)
        {
            Adopt (*row.error);
        }
    }

    Adopt (*m_note);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerRawPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  used = 0;



    SetBounds (boundsDip);
    m_scaler = scaler;

    used = ArrangeRows (boundsDip, scaler, true);
    IGNORE_RETURN_VALUE (used, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawPanel::ArrangeRows
//
//  An error sits under its field and pushes the rows below down by its own
//  height; with no error it takes no room.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerRawPanel::ArrangeRows (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const
{
    int  row    = scaler.ToPx (kRowHeightDip);
    int  gap    = scaler.ToPx (kRowGapDip);
    int  fieldX = bounds.left + scaler.ToPx (kLabelWidthDip);
    int  fieldW = (std::max) ((int) bounds.right - fieldX, 1);
    int  errTop = scaler.ToPx (kErrorTopDip);
    int  y      = bounds.top;



    for (const Row & each : m_rows)
    {
        int  err = (each.error != nullptr) ? GetErrorHeightPx (*each.error, fieldW, scaler) : 0;

        if (!each.field->IsVisible())
        {
            continue;
        }

        if (place)
        {
            each.label->Layout (RECT { bounds.left, y, fieldX, y + row }, scaler);
            each.field->Layout (RECT { fieldX, y, bounds.right, y + row }, scaler);
        }

        y += row + ((err > 0) ? errTop : 0);

        if (place && each.error != nullptr)
        {
            each.error->Layout (RECT { fieldX, y, bounds.right, y + err }, scaler);
        }

        y += err + gap;
    }

    if (place)
    {
        m_note->Layout (RECT { bounds.left, y, bounds.right, y + scaler.ToPx (kNoteHeightDip) }, scaler);
    }

    y += scaler.ToPx (kNoteHeightDip);

    return y - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawPanel::GetErrorHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerRawPanel::GetErrorHeightPx (const DxuiFieldError & error, int widthPx, const DxuiDpiScaler & scaler) const
{
    if (!error.HasError())
    {
        return 0;
    }

    if (m_text == nullptr || m_theme == nullptr)
    {
        return scaler.ToPx (kFallbackErrorDip);
    }

    return error.GetHeightPx (*m_text, *m_theme, scaler, widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawPanel::OnMouse
//
//  A press that lands takes focus to the field it hit.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerRawPanel::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    for (const Row & row : m_rows)
    {
        if (!row.field->IsVisible())
        {
            continue;
        }

        handled = row.field->OnMouse (ev);

        if (handled)
        {
            if (ev.kind == DxuiMouseEventKind::Down && m_onPressed)
            {
                m_onPressed (row.field);
            }

            break;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerRawDialog::Outcome CassoExplorerRawDialog::Ask (HWND owner, const IDxuiTheme * theme, const std::wstring & title, bool sectors, bool reading)
{
    HRESULT                   hr     = S_OK;
    CassoExplorerRawDialog    dialog;
    DxuiWindow::CreateParams  params;
    int                       rows   = (sectors ? 3 : 1) + (reading ? 1 : 0);



    dialog.m_theme   = theme;
    dialog.m_sectors = sectors;
    dialog.m_reading = reading;

    params.title                    = title;
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 440, 130 + CassoExplorerRawPanel::kNoteHeightDip + rows * (CassoExplorerRawPanel::kRowHeightDip + CassoExplorerRawPanel::kRowGapDip) };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return Outcome();
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    return dialog.m_outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerRawDialog::OnCreate()
{
    std::vector<CassoExplorerRawPanel::Row>  rows;



    for (DxuiLabel * label : { &m_startLabel, &m_sectorLabel, &m_countLabel, &m_numberingLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_startLabel.SetText     (m_sectors ? L"Track:" : L"Block:");
    m_sectorLabel.SetText    (L"Sector:");
    m_countLabel.SetText     (m_sectors ? L"Sectors:" : L"Blocks:");
    m_numberingLabel.SetText (L"Numbering:");

    m_note.SetTextRole  (DxuiTextRole::Muted);
    m_note.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_note.SetText      (m_sectors ? (m_reading ? L"Starts at track 17, sector 0, where a DOS 3.3 disk keeps its catalog."
                                                : L"Writing past the end of a sector goes on into the next.")
                                   : (m_reading ? L"Starts at block 2, where a ProDOS disk's volume directory begins."
                                                : L"Writing past the end of a block goes on into the next."));

    for (DxuiTextInput * input : { &m_start, &m_sector, &m_count })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetTextRenderer (GetTextRenderer());
        input->SetMaxLength    (3);
        input->SetOnChange     ([this] (const std::wstring &) { Revalidate(); });
    }

    m_start.SetText  (std::to_wstring (m_sectors ? CassoExplorerRawChoices::kDefaultTrack : CassoExplorerRawChoices::kDefaultBlock));
    m_sector.SetText (std::to_wstring (CassoExplorerRawChoices::kDefaultSector));
    m_count.SetText  (std::to_wstring (m_sectors ? CassoExplorerRawChoices::kDefaultSectorCount : CassoExplorerRawChoices::kDefaultBlockCount));
    m_start.SelectAll();

    //  Logical first: the numbering catalogs, DOS tools and reference books
    //  use. Physical is the order a boot loader reads off the drive.
    m_numbering.SetPopupHost (GetPopupHost());
    m_numbering.SetItems     ({ L"Logical (DOS order)", L"Physical (drive order)" });
    m_numbering.SetSelected  (0);

    rows.push_back (CassoExplorerRawPanel::Row { &m_startLabel, &m_start, &m_startError });

    if (m_sectors)
    {
        rows.push_back (CassoExplorerRawPanel::Row { &m_sectorLabel, &m_sector, &m_sectorError });
    }

    if (m_reading)
    {
        rows.push_back (CassoExplorerRawPanel::Row { &m_countLabel, &m_count, &m_countError });
    }

    if (m_sectors)
    {
        rows.push_back (CassoExplorerRawPanel::Row { &m_numberingLabel, &m_numbering, nullptr });
    }

    m_body = CreateDialogContent<CassoExplorerRawPanel>();
    m_body->Init (std::move (rows), &m_note);
    m_body->SetMeasure (GetTextRenderer(), m_theme);
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    m_ok = AddDialogButton (m_reading ? L"Read" : L"Write", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_ok->SetOnClick ([this]()
    {
        bool  parsed = CassoExplorerRawChoices::TryParse (m_start.GetText(), m_outcome.start);

        parsed = parsed && (!m_sectors || CassoExplorerRawChoices::TryParse (m_sector.GetText(), m_outcome.sector));
        parsed = parsed && (!m_reading || CassoExplorerRawChoices::TryParse (m_count.GetText(), m_outcome.count));

        m_outcome.confirmed = parsed;
        m_outcome.numbering = (m_numbering.GetSelectedIndex() == 1) ? DiskOperations::Numbering::Physical : DiskOperations::Numbering::Logical;
        EndDialog (IDOK);
    });

    m_validator.AddField ([this]()
    {
        return m_sectors ? CassoExplorerRawChoices::ValidateTrack (m_start.GetText()) : CassoExplorerRawChoices::ValidateBlock (m_start.GetText());
    }, &m_startError);

    m_validator.AddField ([this]()
    {
        return m_sectors ? CassoExplorerRawChoices::ValidateSector (m_sector.GetText()) : std::wstring();
    }, &m_sectorError);

    m_validator.AddField ([this]()
    {
        return m_reading ? CassoExplorerRawChoices::ValidateCount (m_count.GetText(), GetAvailable()) : std::wstring();
    }, &m_countError);

    m_validator.SetConfirmButton (m_ok);

    //  Fits the window to its rows once they are first laid out.
    SetDialogTickIntervalMs (50);

    Revalidate();

    SetInitialFocus (&m_start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog::GetAvailable
//
//  How far the disk runs on from the start typed, or zero when it is not one.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerRawDialog::GetAvailable() const
{
    int  start  = -1;
    int  sector = -1;



    if (!CassoExplorerRawChoices::TryParse (m_start.GetText(), start))
    {
        return 0;
    }

    if (!m_sectors)
    {
        return CassoExplorerRawChoices::GetBlocksFrom (start);
    }

    return CassoExplorerRawChoices::TryParse (m_sector.GetText(), sector) ? CassoExplorerRawChoices::GetSectorsFrom (start, sector) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog::Revalidate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerRawDialog::Revalidate()
{
    bool  valid = m_validator.Revalidate();



    IGNORE_RETURN_VALUE (valid, true);

    FitToContent();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog::FitToContent
//
//  The buttons are anchored to the window's bottom, so the window grows or
//  shrinks to keep them just under the rows.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerRawDialog::FitToContent()
{
    RECT  content = {};
    RECT  window  = {};
    int   delta   = 0;



    if (m_body == nullptr || GetHwnd() == nullptr)
    {
        return;
    }

    content = m_body->GetBounds();

    if (content.bottom <= content.top || content.right <= content.left)
    {
        return;
    }

    delta = m_body->GetRequiredHeightPx() - (int) (content.bottom - content.top);

    if (delta != 0 && GetWindowRect (GetHwnd(), &window))
    {
        SetWindowPos (GetHwnd(), nullptr, 0, 0, window.right - window.left, window.bottom - window.top + delta,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerRawDialog::OnDialogTick()
{
    if (!m_fitted && m_body != nullptr && m_body->GetBounds().bottom > m_body->GetBounds().top)
    {
        m_fitted = true;
        FitToContent();
    }
}
