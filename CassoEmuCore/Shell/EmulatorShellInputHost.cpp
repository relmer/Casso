#include "Pch.h"

#include "Shell/EmulatorShellInputHost.h"

#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellRenderer.h"
#include "Ui/Chrome/EmulatorCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::HideTooltips
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::HideTooltips()
{
    m_shell.m_chrome->m_toolbarTooltip.HideImmediate();
    m_shell.m_chrome->m_driveTooltip.HideImmediate();
    m_shell.m_chrome->m_switchBarTooltip.HideImmediate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::SetPlayerPicker
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::SetPlayerPicker (InputModeRules::Picker picker)
{
    RECT  bounds = {};



    m_shell.m_chrome->m_mainMenu.GetCommands().SetPicker (std::move (picker));

    // Straight onto the command bar's drop-down rather than a submenu off the
    // Machine menu: this is a list the user picks from while playing.
    m_shell.m_chrome->m_toolbar.SetDropDownItems (EmulatorCommands::kIdPaddle,
                                                  m_shell.m_chrome->m_mainMenu.GetCommands().GetPaddlePickerItems());

    // The picker wears what is playing, so its width moves with the answer.
    // Without laying the strip out again the new word paints into the rect
    // the old one left behind.
    bounds = m_shell.m_chrome->m_toolbar.GetBounds();

    if (bounds.right > bounds.left)
    {
        m_shell.m_chrome->m_toolbar.Layout (bounds, m_shell.m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::ClaimCaptureBand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::ClaimCaptureBand()
{
    m_shell.m_chrome->m_captureReflowMs = 0;

    if (!m_shell.m_renderer->m_d3dRenderer.IsFullscreen())
    {
        m_shell.m_chrome->ReflowChromeForChangeBand();
        m_shell.m_chrome->m_captureReflowMs = ChangeBannerNowMs();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::ReleaseCaptureBand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::ReleaseCaptureBand()
{
    if (!m_shell.m_renderer->m_d3dRenderer.IsFullscreen())
    {
        m_shell.m_chrome->ReflowChromeForChangeBand();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::IsGuestMouseSuppressed
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::InputHost::IsGuestMouseSuppressed() const
{
    return m_shell.m_scene->m_stripSuppressGuestMouse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::TakeKeyRepeatElapsedUs
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::InputHost::TakeKeyRepeatElapsedUs (uint32_t capUs)
{
    return m_shell.m_renderer->m_frameClock.TakeKeyRepeatElapsedUs (capUs);
}




