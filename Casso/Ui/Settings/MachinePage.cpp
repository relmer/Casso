#include "Pch.h"

#include "MachinePage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeight    = 28;
    constexpr int    s_kLabelWidth   = 140;
    constexpr int    s_kRadioWidth   = 110;
    constexpr int    s_kCheckWidth   = 140;
    constexpr int    s_kSectionGap   = 14;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    std::vector<RadioOption>  BuildRadioRow (int leftPx, int topPx,
                                             const std::vector<std::wstring> & labels)
    {
        std::vector<RadioOption>  out;
        size_t                    i        = 0;
        int                       x        = leftPx;

        for (i = 0; i < labels.size(); ++i)
        {
            RadioOption  opt;
            opt.rect  = MakeRect (x, topPx, s_kRadioWidth, s_kRowHeight);
            opt.label = labels[i];
            out.push_back (std::move (opt));
            x += s_kRadioWidth;
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
    m_machines           = std::move (machines);
    m_activeMachineIndex = activeIndex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Layout (const RECT & rect)
{
    int  x          = rect.left + 16;
    int  y          = rect.top  + 16;
    int  controlsX  = x + s_kLabelWidth;



    m_machineLabel.SetRect (MakeRect (x, y, s_kLabelWidth, s_kRowHeight));
    m_machineLabel.SetText (L"Machine:");
    y += s_kRowHeight + s_kSectionGap;

    m_speedLabel.SetRect (MakeRect (x, y, s_kLabelWidth, s_kRowHeight));
    m_speedLabel.SetText (L"CPU speed:");
    m_speed.SetOptions (BuildRadioRow (controlsX, y,
                                       { L"Authentic", L"2x", L"Maximum" }));
    y += s_kRowHeight + s_kSectionGap;

    m_colorLabel.SetRect (MakeRect (x, y, s_kLabelWidth, s_kRowHeight));
    m_colorLabel.SetText (L"Video:");
    m_color.SetOptions (BuildRadioRow (controlsX, y,
                                       { L"Color", L"Green", L"Amber", L"White" }));
    y += s_kRowHeight + s_kSectionGap;

    m_wpLabel.SetRect (MakeRect (x, y, s_kLabelWidth, s_kRowHeight));
    m_wpLabel.SetText (L"Write protect:");
    m_writeProtect[0].SetRect (MakeRect (controlsX,                   y, s_kCheckWidth, s_kRowHeight));
    m_writeProtect[0].SetLabel (L"Drive 1");
    m_writeProtect[1].SetRect (MakeRect (controlsX + s_kCheckWidth,   y, s_kCheckWidth, s_kRowHeight));
    m_writeProtect[1].SetLabel (L"Drive 2");
    y += s_kRowHeight + s_kSectionGap;

    m_audioLabel.SetRect (MakeRect (x, y, s_kLabelWidth, s_kRowHeight));
    m_audioLabel.SetText (L"Drive audio:");
    m_driveAudio.SetRect (MakeRect (controlsX, y, s_kCheckWidth, s_kRowHeight));
    m_driveAudio.SetLabel (L"Floppy sound on");
    y += s_kRowHeight + s_kSectionGap;

    m_mechLabel.SetRect (MakeRect (x, y, s_kLabelWidth, s_kRowHeight));
    m_mechLabel.SetText (L"Mechanism:");
    m_mechanism.SetOptions (BuildRadioRow (controlsX, y,
                                           { L"Shugart", L"Alps" }));
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

    m_speed.Paint        (painter, text);
    m_color.Paint        (painter, text);
    m_mechanism.Paint    (painter, text);
    m_driveAudio.Paint   (painter, text);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].Paint (painter, text);
    }
}
