#include "Pch.h"

#include "Ui/Settings/SettingsPreviewScene.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellDisks.h"
#include "Ui/Scene/DeskSceneLayout.h"
#include "Ui/Settings/SettingsSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RenderThemePreviewScene
//
//  Draws the desk scene into the theme preview's mock window, so a skeuo theme
//  previews as what it actually is. The 2D paint deliberately left the screen
//  and the drive row out for exactly this.
//
//  The models load a SECOND time here. The shell's scene belongs to the
//  emulator window's device and this sheet is its own window with its own
//  device, so there is nothing to borrow -- and a few thousand triangles is a
//  cheap price for not advertising a presentation the app retired.
//
//  Clipped, because the theme dropdown is allowed to cover the preview and
//  this pass runs after the whole panel tree. The scissor crops rather than
//  re-projects: shrinking the viewport instead would squash the scene into
//  whatever is left.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPreviewScene::Render (ID3D11RenderTargetView * rtv, int widthPx, int heightPx)
{
    ThemePage::PreviewSceneRequest  request;
    DeskSceneComposition            comp;
    HRESULT                         hr       = S_OK;
    int                             fbW      = 0;
    int                             fbH      = 0;
    const uint32_t                * fbPixels = nullptr;
    CrtUvRect                       uv       = { 0.0f, 0.0f, 1.0f, 1.0f };
    RECT                            viewport = {};



    if (m_sheet.m_themePage == nullptr || rtv == nullptr || widthPx <= 0 || heightPx <= 0)
    {
        return;
    }

    request = m_sheet.m_themePage->TakeSceneRequest();

    // This pass runs after the panel tree, outside the sheet's page clip, so
    // a scrolled preview has to be cropped to the viewport here as well.
    if (m_sheet.IsPageScrollable())
    {
        viewport = m_sheet.GetPageViewportPx();

        if (IntersectRect (&request.clipPx, &request.clipPx, &viewport) == FALSE)
        {
            return;
        }
    }

    if (request.mode == ThemePage::PreviewSceneMode::None ||
        request.rectPx.right <= request.rectPx.left ||
        request.clipPx.bottom <= request.clipPx.top)
    {
        return;
    }

    // First frame that wants it: stand the scene up. Tried-once, because a
    // device that cannot carry it will not start working later, and retrying
    // every frame would just burn the failure over and over.
    if (!m_previewSceneReady && !m_previewSceneTried)
    {
        m_previewSceneTried = true;

        if (m_sheet.GetPopupHost() != nullptr)
        {
            hr = m_previewScene.Initialize (m_sheet.GetPopupHost()->GetDevice(), m_sheet.GetPopupHost()->GetContext());

            if (SUCCEEDED (hr))
            {
                HRESULT  hrModels = LoadModels();

                m_previewSceneReady = SUCCEEDED (hrModels);
            }
        }
    }

    if (!m_previewSceneReady)
    {
        return;
    }

    // The machine can change under a staged pick, and the desk wears what the
    // machine wore.
    if (m_sheet.m_emuShell != nullptr && m_sheet.m_emuShell->GetDisks().MachineHasBuiltInDrive() != m_previewSceneIsC)
    {
        hr = LoadModels();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    // The DPI does not set the scene's size -- SolveComposition contain-fits
    // the scene's corners to the viewport and never consults it -- so this
    // only feeds the layout's own dp-based reservations.
    hr = (request.mode == ThemePage::PreviewSceneMode::Full)
       ? DeskSceneLayout::Compute      (request.rectPx, request.dpi, 2, m_previewScene.Metrics(), comp)
       : DeskSceneLayout::ComputeStrip (request.rectPx, request.dpi, 2, m_previewScene.Metrics(), comp);

    if (FAILED (hr) || hr == S_FALSE)
    {
        return;
    }

    m_previewScene.SetComposition (comp);
    m_previewScene.SetClipRect    (&request.clipPx);

    if (request.mode == ThemePage::PreviewSceneMode::Full)
    {
        // The picture comes from the emulator's framebuffer bytes: this
        // device has no CRT chain of its own, and the preview already has the
        // pixels for the flat blit it used to do.
        fbPixels = m_sheet.m_themePage->FramebufferPixels (fbW, fbH);

        if (fbPixels != nullptr && fbW > 0 && fbH > 0)
        {
            hr = m_previewScene.UploadPicture (fbPixels, fbW, fbH);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        hr = m_previewScene.Render (rtv, m_previewScene.PictureSrv(), uv, fbW, fbH);
    }
    else
    {
        hr = m_previewScene.RenderStrip (rtv, comp);
    }

    m_previewScene.SetClipRect (nullptr);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadPreviewSceneModels
//
//  The same pairing rule the shell uses: the //c gets its platinum monitor
//  over matching drives, everything else the beige Monitor II over Disk IIs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPreviewScene::LoadModels()
{
    HRESULT  hr  = S_OK;
    bool     isC = (m_sheet.m_emuShell != nullptr) && m_sheet.m_emuShell->GetDisks().MachineHasBuiltInDrive();



    // The shell has already parsed and baked the pair this machine wears, and
    // the result is device-independent, so take it rather than doing all of
    // that again on the way into the Theme tab.
    CBRA (m_sheet.m_emuShell != nullptr);

    {
        bool  shellHasModels = m_sheet.m_emuShell->GetDeskScene().GetScene().HasModels();

        CBRA (shellHasModels);
    }

    hr = m_previewScene.AdoptModelsFrom (m_sheet.m_emuShell->GetDeskScene().GetScene());
    CHRA (hr);

    m_previewScene.SetPowerLampOn (true);
    m_previewSceneIsC = isC;

Error:
    return hr;
}
