#include "Pch.h"

#include "Ui/DiskInspector/GoToDialog.h"

#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr int  s_kMaxPathChars = 64;





////////////////////////////////////////////////////////////////////////////////
//
//  GoToBody
//
//  The kinds of target in two columns, then the entry row, a line on what
//  the entry may be for the kind chosen, and the error line. A sector, an
//  offset or a cell is entered with its track: "Track [17] Sector [F]".
//
////////////////////////////////////////////////////////////////////////////////

class GoToBody : public DxuiPanel
{
public:
    struct Parts
    {
        DxuiRadioGroup  * kinds      = nullptr;
        DxuiLabel       * trackLabel = nullptr;
        DxuiTextInput   * track      = nullptr;
        DxuiLabel       * entryLabel = nullptr;
        DxuiTextInput   * input      = nullptr;
        DxuiLabel       * hint       = nullptr;
        DxuiLabel       * error      = nullptr;
    };

    void  Init (const Parts & parts, int initial)
    {
        m_parts   = parts;
        m_initial = initial;

        Adopt (*parts.kinds);
        Adopt (*parts.trackLabel);
        Adopt (*parts.track);
        Adopt (*parts.entryLabel);
        Adopt (*parts.input);
        Adopt (*parts.hint);
        Adopt (*parts.error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int                           pad     = scaler.ToPx (kPadDip);
        int                           rowH    = scaler.ToPx (kRowDip);
        int                           labelW  = scaler.ToPx (kLabelDip);
        int                           trackW  = scaler.ToPx (kTrackDip);
        int                           half    = (boundsPx.right - boundsPx.left - 2 * pad) / 2;
        int                           x       = boundsPx.left + pad;
        int                           y       = boundsPx.top  + pad;
        int                           i       = 0;
        int                           rows    = (GoToDialog::kKindCount + 1) / 2;
        int                           sel     = (m_parts.kinds->GetSelected() >= 0) ? m_parts.kinds->GetSelected() : m_initial;
        bool                          isOn    = InspectorGoTo::IsOnTrack (static_cast<GoToKind> (sel));
        std::vector<DxuiRadioOption>  options;



        SetBounds (boundsPx);
        m_boundsPx = boundsPx;
        m_scaler   = scaler;

        for (i = 0; i < GoToDialog::kKindCount; i++)
        {
            options.push_back ({ RECT { x + (i / rows) * half, y + (i % rows) * rowH, x + (i / rows + 1) * half, y + (i % rows + 1) * rowH - 4 },
                                 InspectorGoTo::GetLabel (static_cast<GoToKind> (i)) });
        }

        m_parts.kinds->SetOptions  (std::move (options));
        m_parts.kinds->SetSelected (sel);
        m_parts.kinds->Layout (RECT { x, y, boundsPx.right - pad, y + rows * rowH }, scaler);
        y += rows * rowH + pad;

        m_parts.trackLabel->SetVisible (isOn);
        m_parts.track->SetVisible      (isOn);

        if (isOn)
        {
            m_parts.trackLabel->Layout (RECT { x, y, x + labelW, y + rowH - 4 }, scaler);
            m_parts.track->Layout      (RECT { x + labelW, y, x + labelW + trackW, y + rowH - 4 }, scaler);
            x += labelW + trackW + pad;
        }

        m_parts.entryLabel->Layout (RECT { x, y, x + labelW, y + rowH - 4 }, scaler);
        m_parts.input->Layout      (RECT { x + labelW, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        x  = boundsPx.left + pad;
        y += rowH;

        m_parts.hint->Layout (RECT { x, y, boundsPx.right - pad, y + scaler.ToPx (kHintDip) }, scaler);
        y += scaler.ToPx (kHintDip) + pad / 2;

        m_parts.error->Layout (RECT { x, y, boundsPx.right - pad, y + 2 * rowH }, scaler);
    }

    void  Relayout()
    {
        Layout (m_boundsPx, m_scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        return m_parts.kinds->OnMouse (ev) || (m_parts.track->IsVisible() && m_parts.track->OnMouse (ev)) || m_parts.input->OnMouse (ev);
    }

private:
    static constexpr int  kPadDip   = 12;
    static constexpr int  kRowDip   = 32;
    static constexpr int  kHintDip  = 20;
    static constexpr int  kLabelDip = 56;
    static constexpr int  kTrackDip = 72;

    Parts          m_parts;
    int            m_initial  = 0;
    RECT           m_boundsPx = {};
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::Configure (const IDxuiTheme * theme, const DiskAnalysis & analysis, int quarterTrack, GoToKind kind)
{
    m_theme        = theme;
    m_analysis     = &analysis;
    m_quarterTrack = quarterTrack;
    m_kind         = kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::OnCreate()
{
    GoToBody::Parts  parts;
    DxuiButton *     ok   = nullptr;



    m_kinds.SetDpi      (GetDpi());
    m_kinds.SetSelected (static_cast<int> (m_kind));

    for (DxuiTextInput * input : { &m_track, &m_input })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetMaxLength    (s_kMaxPathChars);
        input->SetTextRenderer (GetTextRenderer());
    }

    m_track.SetText     (InspectorFormat::FormatQuarterTrack (m_quarterTrack));
    m_track.SetOnChange ([this] (const std::wstring &) { ShowHint (GetChosenKind()); });

    m_trackLabel.SetText      (L"Track");
    m_trackLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_entryLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_hint.SetTextRole  (DxuiTextRole::Muted);
    m_hint.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_error.SetTextRole  (DxuiTextRole::Error);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    parts      = { &m_kinds, &m_trackLabel, &m_track, &m_entryLabel, &m_input, &m_hint, &m_error };
    m_body     = CreateDialogContent<GoToBody>();
    m_body->Init (parts, static_cast<int> (m_kind));

    m_kinds.SetOnChange ([this] (int index) { m_body->Relayout(); ShowHint (static_cast<GoToKind> (index)); });
    ShowHint (m_kind);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this] () { OnOkClicked(); });

    SetInitialFocus (&m_input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetChosenKind
//
////////////////////////////////////////////////////////////////////////////////

GoToKind GoToDialog::GetChosenKind() const
{
    return static_cast<GoToKind> (std::clamp (m_kinds.GetSelected(), 0, kKindCount - 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetEnteredTrack
//
//  The quarter track in the Track field, or -1 when it is not a track.
//
////////////////////////////////////////////////////////////////////////////////

int GoToDialog::GetEnteredTrack() const
{
    int  quarterTrack = -1;



    return InspectorFormat::TryParseQuarterTrack (m_track.GetText(), quarterTrack) ? quarterTrack : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::ShowHint
//
//  The entry's label and the line under the entry for a kind, on the entered
//  track, and the last error cleared, since it was about another entry.
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::ShowHint (GoToKind kind)
{
    int  quarterTrack = GetEnteredTrack();



    m_entryLabel.SetText (GetEntryLabel (kind));

    m_hint.SetText ((m_analysis == nullptr)                                       ? std::wstring()
                    : (InspectorGoTo::IsOnTrack (kind) && quarterTrack < 0)        ? std::wstring (L"Enter a track from 0 to 39.75, such as 17.25")
                    : InspectorGoTo::GetRangeHint (*m_analysis, (quarterTrack >= 0) ? quarterTrack : m_quarterTrack, kind));
    m_error.SetText (L"");
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetEntryLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR GoToDialog::GetEntryLabel (GoToKind kind)
{
    LPCWSTR  label = L"Track";



    switch (kind)
    {
        case GoToKind::PhysicalSector: label = L"Sector"; break;
        case GoToKind::Dos33Sector:    label = L"Sector"; break;
        case GoToKind::ProDosBlock:    label = L"Block";  break;
        case GoToKind::NibbleOffset:   label = L"Offset"; break;
        case GoToKind::Cell:           label = L"Cell";   break;
        case GoToKind::File:           label = L"Path";   break;
        default:                                          break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::OnOkClicked
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::OnOkClicked()
{
    std::wstring  error;
    int           quarterTrack = GetEnteredTrack();
    bool          isFound      = false;



    m_kind = GetChosenKind();

    if (InspectorGoTo::IsOnTrack (m_kind) && quarterTrack < 0)
    {
        error = L"Enter a track from 0 to 39.75, such as 17.25.";
    }
    else if (m_analysis != nullptr)
    {
        isFound = InspectorGoTo::Resolve (*m_analysis, (quarterTrack >= 0) ? quarterTrack : m_quarterTrack, m_kind, m_input.GetText(), m_target, error);
    }

    if (isFound)
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