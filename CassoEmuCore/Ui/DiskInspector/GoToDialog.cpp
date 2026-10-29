#include "Pch.h"

#include "Ui/DiskInspector/GoToDialog.h"





static constexpr int  s_kMaxPathChars = 64;





////////////////////////////////////////////////////////////////////////////////
//
//  GoToBody
//
//  The kinds of target in two columns, then the entry, a line on what it may
//  be for the kind chosen, and the error line.
//
////////////////////////////////////////////////////////////////////////////////

class GoToBody : public DxuiPanel
{
public:
    void  Init (DxuiRadioGroup & kinds, DxuiTextInput & input, DxuiLabel & hint, DxuiLabel & error, int initial)
    {
        m_initial = initial;
        m_kinds = &kinds;
        m_input = &input;
        m_hint  = &hint;
        m_error = &error;

        Adopt (kinds);
        Adopt (input);
        Adopt (hint);
        Adopt (error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int                           pad     = scaler.ToPx (kPadDip);
        int                           rowH    = scaler.ToPx (kRowDip);
        int                           half    = (boundsPx.right - boundsPx.left - 2 * pad) / 2;
        int                           x       = boundsPx.left + pad;
        int                           y       = boundsPx.top  + pad;
        int                           i       = 0;
        int                           rows    = (GoToDialog::kKindCount + 1) / 2;
        int                           sel     = (m_kinds->GetSelected() >= 0) ? m_kinds->GetSelected() : m_initial;
        std::vector<DxuiRadioOption>  options;



        SetBounds (boundsPx);

        for (i = 0; i < GoToDialog::kKindCount; i++)
        {
            options.push_back ({ RECT { x + (i / rows) * half, y + (i % rows) * rowH, x + (i / rows + 1) * half, y + (i % rows + 1) * rowH - 4 },
                                 InspectorGoTo::GetLabel (static_cast<GoToKind> (i)) });
        }

        m_kinds->SetOptions  (std::move (options));
        m_kinds->SetSelected (sel);
        m_kinds->Layout (RECT { x, y, boundsPx.right - pad, y + rows * rowH }, scaler);
        y += rows * rowH + pad;

        m_input->Layout (RECT { x, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        y += rowH;

        m_hint->Layout (RECT { x, y, boundsPx.right - pad, y + scaler.ToPx (kHintDip) }, scaler);
        y += scaler.ToPx (kHintDip) + pad / 2;

        m_error->Layout (RECT { x, y, boundsPx.right - pad, y + 2 * rowH }, scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        return m_kinds->OnMouse (ev) || m_input->OnMouse (ev);
    }

private:
    static constexpr int  kPadDip  = 12;
    static constexpr int  kRowDip  = 32;
    static constexpr int  kHintDip = 20;

    DxuiRadioGroup  * m_kinds   = nullptr;
    int               m_initial = 0;
    DxuiTextInput   * m_input   = nullptr;
    DxuiLabel       * m_hint    = nullptr;
    DxuiLabel       * m_error   = nullptr;
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
    GoToBody *    body = nullptr;
    DxuiButton *  ok   = nullptr;



    m_kinds.SetDpi      (GetDpi());
    m_kinds.SetSelected (static_cast<int> (m_kind));

    m_input.SetTheme        (m_theme);
    m_input.SetHwnd         (GetHwnd());
    m_input.SetMaxLength    (s_kMaxPathChars);
    m_input.SetTextRenderer (GetTextRenderer());

    m_hint.SetTextRole  (DxuiTextRole::Muted);
    m_hint.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_kinds.SetOnChange ([this] (int index) { ShowHint (static_cast<GoToKind> (index)); });
    ShowHint (m_kind);

    m_error.SetTextRole  (DxuiTextRole::Error);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    body = CreateDialogContent<GoToBody>();
    body->Init (m_kinds, m_input, m_hint, m_error, static_cast<int> (m_kind));

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this] () { OnOkClicked(); });

    SetInitialFocus (&m_input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::ShowHint
//
//  The line under the entry for a kind, and the last error cleared, since
//  it was about another kind.
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::ShowHint (GoToKind kind)
{
    m_hint.SetText ((m_analysis != nullptr) ? InspectorGoTo::GetRangeHint (*m_analysis, m_quarterTrack, kind) : std::wstring());
    m_error.SetText (L"");
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::OnOkClicked
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::OnOkClicked()
{
    std::wstring  error;



    m_kind = static_cast<GoToKind> (std::clamp (m_kinds.GetSelected(), 0, kKindCount - 1));

    if (m_analysis != nullptr && InspectorGoTo::Resolve (*m_analysis, m_quarterTrack, m_kind, m_input.GetText(), m_target, error))
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
