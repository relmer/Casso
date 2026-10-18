#include "Pch.h"

#include "Shell/EmulatorShellInputHost.h"

#include "Shell/EmulatorShellInternal.h"
#include "Shell/ClipboardManager.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellSettings.h"
#include "Ui/Chrome/EmulatorCommands.h"
#include "Ui/Settings/SettingsSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::GetMainWindow
//
////////////////////////////////////////////////////////////////////////////////

HWND EmulatorShell::InputHost::GetMainWindow() const
{
    return m_shell.m_hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::MapPointerToPicture
//
////////////////////////////////////////////////////////////////////////////////

std::optional<uint32_t> EmulatorShell::InputHost::MapPointerToPicture (int xPx, int yPx)
{
    const RECT      & vp       = m_shell.m_viewportBoundsPx;
    int               vpW      = vp.right  - vp.left;
    int               vpH      = vp.bottom - vp.top;
    bool              isInside = xPx >= vp.left && xPx < vp.right && yPx >= vp.top && yPx < vp.bottom;
    uint32_t          fx       = 0;
    uint32_t          fy       = 0;
    SceneHitResult    hit;



    if (vpW <= 1 || vpH <= 1)
    {
        return std::nullopt;
    }

    // Curvature-correct mapping: the pixel comes from the inverse projection
    // through the glass, so only the picture counts -- positions off the
    // glass, including what used to be letterbox bars, are off it.
    if (m_shell.CrtMonitorActive())
    {
        hit = m_shell.m_scene->DeskSceneHit (xPx, yPx);

        if (hit.target != SceneHitResult::Target::Glass)
        {
            return std::nullopt;
        }

        fx = static_cast<uint32_t> (MulDiv (hit.emulatedPixel.x, 65535, kFramebufferWidth - 1));
        fy = static_cast<uint32_t> (MulDiv (hit.emulatedPixel.y, 65535, kFramebufferHeight - 1));
        return (fx << 16) | fy;
    }

    if (!isInside)
    {
        return std::nullopt;
    }

    fx = static_cast<uint32_t> (MulDiv (xPx - vp.left, 65535, vpW - 1));
    fy = static_cast<uint32_t> (MulDiv (yPx - vp.top,  65535, vpH - 1));

    return (fx << 16) | fy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::HideTooltips
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::HideTooltips()
{
    m_shell.m_chrome->HideTooltipsNow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::SetPlayerPicker
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::SetPlayerPicker (InputModeRules::Picker picker)
{
    RECT  bounds = {};



    m_shell.m_chrome->GetMainMenu().GetCommands().SetPicker (std::move (picker));

    // Straight onto the command bar's drop-down rather than a submenu off the
    // Machine menu: this is a list the user picks from while playing.
    m_shell.m_chrome->GetToolbar().SetDropDownItems (EmulatorCommands::kIdPaddle,
                                                  m_shell.m_chrome->GetMainMenu().GetCommands().GetPaddlePickerItems());

    // The picker wears what is playing, so its width moves with the answer.
    // Without laying the strip out again the new word paints into the rect
    // the old one left behind.
    bounds = m_shell.m_chrome->GetToolbar().GetBounds();

    if (bounds.right > bounds.left)
    {
        m_shell.m_chrome->GetToolbar().Layout (bounds, m_shell.m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::ClaimCaptureBand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::ClaimCaptureBand()
{
    m_shell.m_chrome->MarkCaptureReflow (0);

    if (!m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        m_shell.m_chrome->ReflowChromeForChangeBand();
        m_shell.m_chrome->MarkCaptureReflow (ChangeBannerNowMs());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::ReleaseCaptureBand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::ReleaseCaptureBand()
{
    if (!m_shell.m_renderer->GetD3D().IsFullscreen())
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
    return m_shell.m_scene->IsGuestMouseSuppressed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::ShowNotice
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::ShowNotice (const std::wstring & text)
{
    m_shell.m_chrome->ShowNotice (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::PostNotice
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::PostNotice (const std::wstring & text)
{
    m_shell.m_chrome->PostNotice (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::SwallowNextChar
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::SwallowNextChar()
{
    m_shell.m_swallowMetaChar = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::TakeKeyRepeatElapsedUs
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::InputHost::TakeKeyRepeatElapsedUs (uint32_t capUs)
{
    return m_shell.m_renderer->TakeKeyRepeatElapsedUs (capUs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::GetClipboard
//
////////////////////////////////////////////////////////////////////////////////

ClipboardManager * EmulatorShell::InputHost::GetClipboard()
{
    return m_shell.m_clipboardManager.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::StartNewControllerProfile
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::StartNewControllerProfile (ControllerUnitKey unit)
{
    m_shell.m_settings->OpenSettings (false);

    if (m_shell.m_settings->GetSheet() != nullptr)
    {
        m_shell.m_settings->GetSheet()->StartNewControllerProfile (unit);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

GlobalUserPrefs & EmulatorShell::InputHost::GetPrefs()
{
    return m_shell.m_settings->GetPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::SavePrefs
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::SavePrefs()
{
    m_shell.m_settings->SaveGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::GetConfigStore
//
////////////////////////////////////////////////////////////////////////////////

UserConfigStore * EmulatorShell::InputHost::GetConfigStore()
{
    return m_shell.m_settings->GetConfigStore();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::GetFileSystem
//
////////////////////////////////////////////////////////////////////////////////

IFileSystem & EmulatorShell::InputHost::GetFileSystem()
{
    return m_shell.m_settings->GetFileSystem();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost::LoadMachineUiPrefs
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InputHost::LoadMachineUiPrefs (JsonValue & outDoc, const JsonValue * & outUiPrefs)
{
    m_shell.m_settings->LoadMachineUiPrefs (outDoc, outUiPrefs);
}
