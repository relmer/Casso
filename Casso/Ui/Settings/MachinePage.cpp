#include "Pch.h"

#include "MachinePage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp    = 28;
    constexpr int    s_kLabelWidthDp   = 140;
    constexpr int    s_kRadioWidthDp   = 110;
    constexpr int    s_kCheckWidthDp   = 140;
    constexpr int    s_kDropdownWidthDp = 260;
    constexpr int    s_kSectionGapDp   = 14;
    constexpr int    s_kPagePadDp      = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    std::vector<RadioOption>  BuildRadioRow (const DpiScaler & scaler,
                                             int leftPx, int topPx,
                                             const std::vector<std::wstring> & labels)
    {
        std::vector<RadioOption>  out;
        size_t                    i           = 0;
        int                       radioWidth  = scaler.Px (s_kRadioWidthDp);
        int                       rowHeight   = scaler.Px (s_kRowHeightDp);
        int                       x           = leftPx;

        for (i = 0; i < labels.size(); ++i)
        {
            RadioOption  opt;
            opt.rect  = MakeRect (x, topPx, radioWidth, rowHeight);
            opt.label = labels[i];
            out.push_back (std::move (opt));
            x += radioWidth;
        }
        return out;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetMachineList
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetMachineList (std::vector<std::string> machines, int activeIndex)
{
    std::vector<std::wstring>  labels;



    m_machines           = std::move (machines);
    m_activeMachineIndex = activeIndex;

    for (const std::string & machine : m_machines)
    {
        labels.emplace_back (machine.begin(), machine.end());
    }

    m_machineDropdown.SetItems    (labels);
    m_machineDropdown.SetSelected (m_activeMachineIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Layout (const RECT & rect, const DpiScaler & scaler)
{
    UINT dpi          = scaler.Dpi();
    int  pad          = scaler.Px (s_kPagePadDp);
    int  rowHeight    = scaler.Px (s_kRowHeightDp);
    int  labelWidth   = scaler.Px (s_kLabelWidthDp);
    int  checkWidth   = scaler.Px (s_kCheckWidthDp);
    int  dropWidth    = scaler.Px (s_kDropdownWidthDp);
    int  sectionGap   = scaler.Px (s_kSectionGapDp);
    int  x            = rect.left + pad;
    int  y            = rect.top  + pad;
    int  controlsX    = x + labelWidth;



    m_machineLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_machineLabel.SetText (L"Machine:");
    m_machineDropdown.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_speedLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_speedLabel.SetText (L"CPU speed:");
    m_speed.SetOptions (BuildRadioRow (scaler, controlsX, y,
                                       { L"Authentic", L"2x", L"Maximum" }));
    y += rowHeight + sectionGap;

    m_colorLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_colorLabel.SetText (L"Video:");
    m_color.SetOptions (BuildRadioRow (scaler, controlsX, y,
                                       { L"Color", L"Green", L"Amber", L"White" }));
    y += rowHeight + sectionGap;

    m_wpLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_wpLabel.SetText (L"Write protect:");
    m_writeProtect[0].SetRect (MakeRect (controlsX,                y, checkWidth, rowHeight));
    m_writeProtect[0].SetLabel (L"Drive 1");
    m_writeProtect[1].SetRect (MakeRect (controlsX + checkWidth,   y, checkWidth, rowHeight));
    m_writeProtect[1].SetLabel (L"Drive 2");
    y += rowHeight + sectionGap;

    m_audioLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_audioLabel.SetText (L"Drive audio:");
    m_driveAudio.SetRect (MakeRect (controlsX, y, checkWidth, rowHeight));
    m_driveAudio.SetLabel (L"Floppy sound on");
    y += rowHeight + sectionGap;

    m_mechLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_mechLabel.SetText (L"Mechanism:");
    m_mechanism.SetOptions (BuildRadioRow (scaler, controlsX, y,
                                           { L"Shugart", L"Alps" }));

    m_machineLabel.SetDpi    (dpi);
    m_speedLabel.SetDpi      (dpi);
    m_colorLabel.SetDpi      (dpi);
    m_wpLabel.SetDpi         (dpi);
    m_audioLabel.SetDpi      (dpi);
    m_mechLabel.SetDpi       (dpi);
    m_machineDropdown.SetDpi (dpi);
    m_speed.SetDpi           (dpi);
    m_color.SetDpi           (dpi);
    m_mechanism.SetDpi       (dpi);
    m_driveAudio.SetDpi      (dpi);
    m_writeProtect[0].SetDpi (dpi);
    m_writeProtect[1].SetDpi (dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Rebuild
//
//  Re-sync widget visible state to the underlying SettingsPanelState
//  and wire each widget's OnChange callback back into the state.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Rebuild ()
{
    SettingsPanelState  * state = m_state;



    if (state == nullptr)
    {
        return;
    }

    m_speed.SetSelected     ((int) state->Prefs().speedMode);
    m_color.SetSelected     ((int) state->Prefs().colorMode);
    m_mechanism.SetSelected (state->Prefs().floppyMechanism == "alps" ? 1 : 0);
    m_driveAudio.SetChecked (state->Prefs().floppySoundEnabled);
    m_writeProtect[0].SetChecked (state->Prefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (state->Prefs().writeProtect[1]);
    m_machineDropdown.SetSelected (m_activeMachineIndex);

    m_machineDropdown.SetSelect ([this] (int idx)
    {
        if (idx >= 0 && idx < (int) m_machines.size())
        {
            m_activeMachineIndex = idx;
            if (m_onMachineSelected)
            {
                m_onMachineSelected (m_machines[(size_t) idx]);
            }
        }
    });
    m_speed.SetOnChange      ([state] (int idx) { state->SetSpeedMode ((SettingsSpeedMode) idx); });
    m_color.SetOnChange      ([state] (int idx) { state->SetColorMode ((SettingsColorMode) idx); });
    m_mechanism.SetOnChange  ([state] (int idx) { state->SetMechanism (idx == 1 ? "alps" : "shugart"); });
    m_driveAudio.SetOnChange ([state] (bool checked) { state->SetFloppySound (checked); });
    m_writeProtect[0].SetOnChange ([state] (bool checked) { state->SetWriteProtect (0, checked); });
    m_writeProtect[1].SetOnChange ([state] (bool checked) { state->SetWriteProtect (1, checked); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnLButtonDown (int x, int y)
{
    int   i = 0;

    if (m_machineDropdown.OnLButtonDown (x, y)) { return; }
    if (m_speed.OnLButtonDown     (x, y)) { return; }
    if (m_color.OnLButtonDown     (x, y)) { return; }
    if (m_mechanism.OnLButtonDown (x, y)) { return; }
    if (m_driveAudio.OnLButtonDown (x, y)) { return; }
    for (i = 0; i < 2; ++i)
    {
        if (m_writeProtect[(size_t) i].OnLButtonDown (x, y)) { return; }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnLButtonUp (int x, int y)
{
    int  i = 0;

    (void) m_machineDropdown.OnLButtonUp (x, y);
    (void) m_speed.OnLButtonUp     (x, y);
    (void) m_color.OnLButtonUp     (x, y);
    (void) m_mechanism.OnLButtonUp (x, y);
    (void) m_driveAudio.OnLButtonUp (x, y);
    for (i = 0; i < 2; ++i)
    {
        (void) m_writeProtect[(size_t) i].OnLButtonUp (x, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnMouseHover (int x, int y)
{
    int  i = 0;

    m_machineDropdown.SetMouseHover (x, y);
    m_speed.SetMouseHover     (x, y);
    m_color.SetMouseHover     (x, y);
    m_mechanism.SetMouseHover (x, y);
    m_driveAudio.SetMouseHover (x, y);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].SetMouseHover (x, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool MachinePage::OnKey (WPARAM vk)
{
    int  i = 0;

    if (m_machineDropdown.HandleKey (vk)) { return true; }
    if (m_speed.OnKey      (vk)) { return true; }
    if (m_color.OnKey      (vk)) { return true; }
    if (m_mechanism.OnKey  (vk)) { return true; }
    if (m_driveAudio.OnKey (vk)) { return true; }
    for (i = 0; i < 2; ++i)
    {
        if (m_writeProtect[(size_t) i].OnKey (vk)) { return true; }
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    int  i = 0;

    m_machineLabel.Paint (painter, text);
    m_speedLabel.Paint   (painter, text);
    m_colorLabel.Paint   (painter, text);
    m_wpLabel.Paint      (painter, text);
    m_audioLabel.Paint   (painter, text);
    m_mechLabel.Paint    (painter, text);

    m_machineDropdown.Paint (painter, text);
    m_speed.Paint        (painter, text);
    m_color.Paint        (painter, text);
    m_mechanism.Paint    (painter, text);
    m_driveAudio.Paint   (painter, text);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].Paint (painter, text);
    }
}
