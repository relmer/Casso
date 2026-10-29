#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView
//
//  A picture preview on its monitor, through Casso's CRT pass. A zoomed
//  picture pans by dragging, its scrollbars and the wheel.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewPictureView : public PreviewView
{
public:
    PreviewPictureView (ExplorerPreviewManager & manager, ExplorerContext & context);

    void                       Create               () override;
    void                       Configure            () override;
    void                       Layout               (const RECT & content) override;
    void                       Hide                 () override;
    bool                       UpdateScrollbarHover (POINT at) override;
    bool                       TickScrollbars       (int64_t nowMs) override;
    void                       Fill                 (const PreviewContent & preview, bool picture, bool keepShellPicture);
    void                       ShowFramebuffer      (const std::vector<uint32_t> & bgra, int width, int height);
    void                       Clear                ();
    void                       SetVisible           (bool visible);
    void                       SetZoom              (float zoom);
    LPCWSTR                    GetCursorForPoint    (POINT clientPx) const;
    bool                       TryGetScrollbarMenu  (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const;
    PicturePreview::Monitor    GetMonitor           () const;
    const ThemeCrtDefaults   * GetCrtDefaults       ();
    void                       UpdateCrtPicture     (bool force);
    void                       SelectMonitor        (PicturePreview::Monitor monitor);
    void                       ShowForm             (bool color);
    void                       SavePicture          ();

    //  The PNG save picker's client GUID, so its history is separate.
    static constexpr GUID  s_kSavePictureGuid = { 0xb7f3266b, 0x19b9, 0x45f0, { 0xa5, 0xd0, 0x58, 0x1a, 0x63, 0xf2, 0xda, 0xf7 } };

    bool                      HandleMouse         (const DxuiMouseEvent & ev);

private:
    void  ShowContextMenu (int x, int y);

    DxuiFramebufferView  * m_picture       = nullptr;
    CrtPictureRenderer     m_crtPicture;
    int                    m_crtWidth      = 0;
    int                    m_crtHeight     = 0;
    std::string            m_crtThemeName;
    LoadedTheme            m_crtTheme;
    bool                   m_crtThemeValid = false;
};
