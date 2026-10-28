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
