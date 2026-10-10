#include "Pch.h"

#include "Ui/DiskInspector/ExportDialog.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr int  s_kMaxTrackChars = 2;
static constexpr int  s_kLastTrack     = DiskImage::kQuarterTrackCount / DiskImage::kQuarterTracksPerWholeTrack - 1;

static constexpr LPCWSTR  s_kpszForms[]  = { L"Sectors", L"Track nibbles", L"Track bits (WOZ)" };
static constexpr LPCWSTR  s_kpszScopes[] = { L"Selected sector", L"Selected track", L"Tracks" };
static constexpr LPCWSTR  s_kpszOrders[] = { L"Physical", L"DOS 3.3", L"ProDOS" };





////////////////////////////////////////////////////////////////////////////////
//
//  ExportBody
//
//  A row of choices per question, the track range, then the error line.
//
////////////////////////////////////////////////////////////////////////////////

class ExportBody : public DxuiPanel
{
public:
    void  Init (DxuiRadioGroup & form, DxuiRadioGroup & scope, DxuiRadioGroup & order, DxuiTextInput & first, DxuiTextInput & last, DxuiLabel & error)
    {
        m_form  = &form;
        m_scope = &scope;
        m_order = &order;
        m_first = &first;
        m_last  = &last;
        m_error = &error;

        Adopt (form);
        Adopt (scope);
        Adopt (order);
        Adopt (first);
        Adopt (last);
        Adopt (error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int  pad  = scaler.ToPx (kPadDip);
        int  rowH = scaler.ToPx (kRowDip);
        int  w    = (boundsPx.right - boundsPx.left - 2 * pad) / 3;
        int  x    = boundsPx.left + pad;
        int  y    = boundsPx.top  + pad;



        SetBounds (boundsPx);

        LayoutRow (*m_form,  s_kpszForms,  x, y, w, rowH, scaler);
        y += rowH + pad;
        LayoutRow (*m_scope, s_kpszScopes, x, y, w, rowH, scaler);
        y += rowH;
        m_first->Layout (RECT { x + 2 * w, y, x + 2 * w + w / 2 - 4, y + rowH - 4 }, scaler);
        m_last->Layout  (RECT { x + 2 * w + w / 2, y, x + 3 * w, y + rowH - 4 }, scaler);
        y += rowH + pad;
        LayoutRow (*m_order, s_kpszOrders, x, y, w, rowH, scaler);
        y += rowH + pad;
        m_error->Layout (RECT { x, y, boundsPx.right - pad, y + 2 * rowH }, scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        return m_form->OnMouse (ev) || m_scope->OnMouse (ev) || m_order->OnMouse (ev) || m_first->OnMouse (ev) || m_last->OnMouse (ev);
    }

private:
    static constexpr int  kPadDip = 12;
    static constexpr int  kRowDip = 32;

    static void  LayoutRow (DxuiRadioGroup & group, const LPCWSTR (& labels)[3], int x, int y, int w, int rowH, const DxuiDpiScaler & scaler)
    {
        int                           sel     = group.GetSelected();
        std::vector<DxuiRadioOption>  options;



        for (int i = 0; i < 3; i++)
        {
            options.push_back ({ RECT { x + i * w, y, x + (i + 1) * w, y + rowH - 4 }, labels[i] });
        }

        group.SetOptions  (std::move (options));
        group.SetSelected (sel);
        group.Layout (RECT { x, y, x + 3 * w, y + rowH - 4 }, scaler);
    }

    DxuiRadioGroup *  m_form  = nullptr;
    DxuiRadioGroup *  m_scope = nullptr;
    DxuiRadioGroup *  m_order = nullptr;
    DxuiTextInput *   m_first = nullptr;
    DxuiTextInput *   m_last  = nullptr;
    DxuiLabel *       m_error = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExportDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExportDialog::Configure (const IDxuiTheme * theme, const DiskAnalysis & analysis, int quarterTrack, int sectorIndex, const std::wstring & imageName)
{
    m_theme        = theme;
    m_analysis     = &analysis;
    m_quarterTrack = quarterTrack;
    m_sectorIndex  = sectorIndex;
    m_imageName    = imageName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExportDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void ExportDialog::OnCreate()
{
    ExportBody *  body  = nullptr;
    DxuiButton *  ok    = nullptr;
    std::wstring  track = std::to_wstring (m_quarterTrack / DiskImage::kQuarterTracksPerWholeTrack);



    m_form.SetDpi      (GetDpi());
    m_scope.SetDpi     (GetDpi());
    m_order.SetDpi     (GetDpi());
    m_form.SetSelected  (0);
    m_scope.SetSelected (m_sectorIndex >= 0 ? 0 : 1);
    m_order.SetSelected (0);

    for (DxuiTextInput * input : { &m_firstTrack, &m_lastTrack })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetMaxLength    (s_kMaxTrackChars);
        input->SetTextRenderer (GetTextRenderer());
        input->SetText         (track);
    }

    m_error.SetTextRole  (DxuiTextRole::Error);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    body = CreateDialogContent<ExportBody>();
    body->Init (m_form, m_scope, m_order, m_firstTrack, m_lastTrack, m_error);

    ok = AddDialogButton (L"Export...", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this] () { OnOkClicked(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExportDialog::OnOkClicked
//
////////////////////////////////////////////////////////////////////////////////

void ExportDialog::OnOkClicked()
{
    ExportForm    form  = static_cast<ExportForm> (std::clamp (m_form.GetSelected(), 0, 2));
    std::wstring  error;
    bool          isOk  = false;



    m_request = ExportRequest();
    isOk      = (form == ExportForm::Sectors) ? BuildSectors (error) : BuildTrack (form, error);

    if (isOk)
    {
        m_isChosen = true;
        EndDialog (IDOK);
    }
    else
    {
        m_error.SetText (error);
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExportDialog::BuildSectors
//
////////////////////////////////////////////////////////////////////////////////

bool ExportDialog::BuildSectors (std::wstring & outError)
{
    SectorOrder            order    = static_cast<SectorOrder> (std::clamp (m_order.GetSelected(), 0, 2));
    int                    scope    = m_scope.GetSelected();
    int                    track    = m_quarterTrack / DiskImage::kQuarterTracksPerWholeTrack;
    int                    first    = track;
    int                    last     = track;
    int                    slot     = m_analysis->entries[m_quarterTrack].slot;
    vector<ExportProblem>  problems;



    if (scope == 2 && (!InspectorFormat::TryParseDecimal (m_firstTrack.GetText(), first) || !InspectorFormat::TryParseDecimal (m_lastTrack.GetText(), last) ||
                       first < 0 || last > s_kLastTrack || first > last))
    {
        outError = std::format (L"Enter whole tracks from 0 to {}, the first no later than the last.", s_kLastTrack);
    }
    else if (scope == 0 && (m_sectorIndex < 0 || slot < 0))
    {
        outError = L"No sector is selected.";
    }
    else if (scope == 0)
    {
        TrackExport::ExportSector (*m_analysis->tracks[slot], track, m_sectorIndex, m_request.bytes, problems);
    }
    else
    {
        TrackExport::ExportSectors (*m_analysis, first, last, order, m_request.bytes, problems);
    }

    for (const ExportProblem & problem : problems)
    {
        m_request.notes.push_back (std::format (L"Track {}, {}{}", problem.track, problem.sector >= 0 ? L"sector " + InspectorFormat::FormatSector (problem.sector) + L": " : L"",
                                                problem.reason));
    }

    m_request.fileName  = TrackExport::GetSectorsName (m_imageName, first, last);
    m_request.extension = L"bin";

    return outError.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExportDialog::BuildTrack
//
////////////////////////////////////////////////////////////////////////////////

bool ExportDialog::BuildTrack (ExportForm form, std::wstring & outError)
{
    HRESULT  hr   = S_OK;
    int      slot = m_analysis->entries[m_quarterTrack].slot;



    if (slot < 0 || slot >= static_cast<int> (m_analysis->tracks.size()) || m_analysis->copy == nullptr)
    {
        outError = L"Nothing is recorded on this quarter track.";
    }
    else if (form == ExportForm::TrackNibbles)
    {
        TrackExport::ExportNibbles (*m_analysis->tracks[slot], m_request.bytes, m_request.notes);
        m_request.fileName  = TrackExport::GetNibblesName (m_imageName, m_quarterTrack);
        m_request.extension = L"bin";
    }
    else
    {
        hr = TrackExport::ExportTrackBits (*m_analysis->copy->tracks[slot], m_quarterTrack, m_request.bytes);
        outError            = SUCCEEDED (hr) ? L"" : L"The track could not be written as a WOZ file.";
        m_request.fileName  = TrackExport::GetBitsName (m_imageName, m_quarterTrack);
        m_request.extension = L"woz";
    }

    return outError.empty();
}
