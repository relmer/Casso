#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewPictureView.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Core/TextEncoding.h"
#include "Config/Win32FileSystem.h"
#include "Core/PathResolver.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::PreviewPictureView
//
////////////////////////////////////////////////////////////////////////////////

PreviewPictureView::PreviewPictureView (ExplorerPreviewManager & manager, ExplorerContext & context)
    : PreviewView (manager, context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::Create
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::Create()
{
    CassoExplorerNamedControl<DxuiFramebufferView>  * picture = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiFramebufferView>>();



    picture->SetAccessibleName (L"Picture preview");
    m_picture = picture;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::Configure
//
//  A picture fills the pane at any size, as Explorer's preview does, not
//  only at whole multiples of its own.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::Configure()
{
    m_picture->SetZoom    ((float) m_context.prefs.previewZoom / 100.0f);
    m_picture->SetScaling (false, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::Layout (const RECT & content)
{
    int  insetPx = m_context.scaler.ToPx (ExplorerMetrics::kPictureInsetDip);



    m_picture->Layout (RECT { content.left + insetPx, content.top + insetPx, content.right - insetPx, content.bottom - insetPx }, m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::Hide()
{
    m_picture->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::UpdateScrollbarHover
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewPictureView::UpdateScrollbarHover (POINT at)
{
    return m_picture->SetScrollbarHover (at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewPictureView::TickScrollbars (int64_t nowMs)
{
    return m_picture->TickScrollbars (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::Fill
//
//  A picture preview's frame, drawn through the CRT pass. Any other preview
//  clears the view, unless the view is showing a shell preview's thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::Fill (const PreviewContent & preview, bool picture, bool keepShellPicture)
{
    if (picture)
    {
        m_picture->SetFramebuffer (preview.bgra.data(), preview.width, preview.height);
        UpdateCrtPicture (true);
    }
    else if (!keepShellPicture)
    {
        m_picture->Clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::ShowFramebuffer
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::ShowFramebuffer (const std::vector<uint32_t> & bgra, int width, int height)
{
    m_picture->SetFramebuffer (bgra.data(), width, height);
    m_picture->SetVisible (m_context.prefs.previewVisible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::Clear
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::Clear()
{
    m_picture->Clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::SetVisible
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::SetVisible (bool visible)
{
    m_picture->SetVisible (visible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::SetZoom (float zoom)
{
    m_picture->SetZoom (zoom);
    UpdateCrtPicture (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::HandleMouse
//
//  A double hi-res picture's menu switches it between color and its bits,
//  whichever the file's name or dots chose. A zoomed picture keeps the
//  pointer while a drag is under way.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewPictureView::HandleMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    if (press && ev.button == DxuiMouseButton::Right && m_picture->IsVisible() && ExplorerLayout::Contains (m_picture->GetBounds(), point)
        && m_context.browser.CanTogglePictureColor())
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
        ShowContextMenu (point.x, point.y);
        return true;
    }

    if (!m_picture->IsInteracting()
        && !(m_picture->IsVisible() && ExplorerLayout::Contains (m_picture->GetBounds(), point) && m_picture->OnMouse (ev)))
    {
        return false;
    }

    if (m_picture->IsInteracting())
    {
        m_picture->OnMouse (ev);
    }

    if (press)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::GetCursorForPoint
//
//  ExplorerInput::GetCursorForPoint calls this after the address bar's and
//  the preview splitter's lookups.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PreviewPictureView::GetCursorForPoint (POINT clientPx) const
{
    if (!m_picture->IsVisible() || !ExplorerLayout::Contains (m_picture->GetBounds(), clientPx))
    {
        return nullptr;
    }

    return m_picture->GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::TryGetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewPictureView::TryGetScrollbarMenu (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const
{
    if (!m_picture->IsVisible() || !ExplorerLayout::Contains (m_picture->GetBounds(), ev.positionDip))
    {
        return false;
    }

    outItems = m_picture->GetScrollbarMenu (ev.positionDip);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::GetMonitor
//
//  The monitor chosen in Casso Explorer, or green, the phosphor of Casso's
//  monitors, when none has been.
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor PreviewPictureView::GetMonitor() const
{
    for (int index = 0; index < PicturePreview::kMonitorCount; index++)
    {
        if (m_context.prefs.previewMonitor == CassoExplorerPrefs::kMonitorNames[index])
        {
            return (PicturePreview::Monitor) index;
        }
    }

    return PicturePreview::Monitor::Green;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::GetCrtDefaults
//
//  The effects of the Casso theme Casso Explorer is in, as Casso extracts it
//  to its folder; the retro theme's under Light, Dark and Follow Windows.
//  Null, the effects' defaults, when Casso has not extracted its themes.
//
////////////////////////////////////////////////////////////////////////////////

const ThemeCrtDefaults * PreviewPictureView::GetCrtDefaults()
{
    bool             casso = ExplorerThemes::IsCassoThemeName (m_context.prefs.theme);
    std::string      name  = casso ? m_context.prefs.theme : std::string (CassoExplorerPrefs::kThemeRetroTerminal);
    Win32FileSystem  fs;
    ThemeLoadError   error;
    std::wstring     dir;
    HRESULT          hr    = S_OK;



    if (name != m_crtThemeName)
    {
        m_crtThemeName  = name;
        m_crtTheme      = LoadedTheme();
        dir             = (PathResolver::GetLocalAppDataDir (L"Casso") / L"Themes" / TextEncoding::Utf8ToWide (name)).wstring();
        hr              = ThemeLoader::Load (fs, dir, m_crtTheme, error);
        m_crtThemeValid = SUCCEEDED (hr);
    }

    return m_crtThemeValid ? &m_crtTheme.crtDefaults : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::UpdateCrtPicture
//
//  The picture drawn through Casso's CRT pass on the picture's monitor, at
//  the view's drawn size, again only when that size changed unless `force`.
//  A preview has a single frame, so persistence, which blends a frame into
//  the next, is left out. The picture's glow spills past its edges, into a
//  black margin drawn around it.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::UpdateCrtPicture (bool force)
{
    const PreviewContent   & preview  = m_context.browser.GetPreview();
    int                      width    = 0;
    int                      height   = 0;
    int                      margin   = 0;
    CrtResolved              resolved;
    CrtParams                params;
    std::vector<uint32_t>    drawn;
    HRESULT                  hr       = S_OK;
    auto                     found    = m_context.prefs.crtOverrides.end();



    if (preview.kind != PreviewContent::Kind::Picture || preview.bgra.empty() || m_context.window.GetPopupHost() == nullptr)
    {
        return;
    }

    m_picture->GetScaledSize (width, height);

    if (width <= 0 || height <= 0 || (!force && width == m_crtWidth && height == m_crtHeight))
    {
        return;
    }

    found    = m_context.prefs.crtOverrides.find (CassoExplorerPrefs::kMonitorNames[(int) m_context.browser.GetPictureMonitor()]);
    resolved = CrtResolver::Resolve (CrtPresets::GetPreset (PicturePreview::GetPresetIndex (m_context.browser.GetPictureMonitor())), GetCrtDefaults(),
                                     (found != m_context.prefs.crtOverrides.end()) ? found->second : CrtOverrides());
    params             = MakeCrtParams (resolved, (float) width, (float) height);
    params.persistence = 0.0f;

    hr = m_crtPicture.Render (m_context.window.GetPopupHost()->GetDevice(), m_context.window.GetPopupHost()->GetContext(), preview.bgra, preview.width, preview.height,
                              width, height, params, drawn, margin);

    if (SUCCEEDED (hr))
    {
        m_picture->SetDisplayImage (std::move (drawn), width + 2 * margin, height + 2 * margin, margin);
        m_crtWidth  = width;
        m_crtHeight = height;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::SelectMonitor
//
//  The picture drawn again on the monitor, in the custom monitor's saved
//  color for that one, which is saved for the next start, apart from
//  Casso's.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::SelectMonitor (PicturePreview::Monitor monitor)
{
    m_context.prefs.previewMonitor = CassoExplorerPrefs::kMonitorNames[(int) monitor];
    m_context.browser.SetPreviewMonitor (monitor, m_context.prefs.previewCustomArgb);
    m_manager.FillPreview();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::ShowForm
//
//  A double hi-res picture drawn in color or as its bits, on the monitor that
//  form is drawn on; the form already drawn is left as it is, so one of the
//  two is always the one drawn.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::ShowForm (bool color)
{
    if ((m_context.browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes) != color && m_context.browser.TogglePictureColor())
    {
        m_manager.FillPreview();
        m_context.window.Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewPictureView::ShowContextMenu
//
//  Color and Monochrome for a double hi-res picture, one of them checked;
//  the choice lasts for this picture.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewPictureView::ShowContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;
    bool                            color = m_context.browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes;



    m_context.menus.GetMenuCommands().clear();

    for (bool asColor : { true, false })
    {
        m_context.menus.AddMenuCommand (items, asColor ? L"&Color" : L"&Monochrome", [this, asColor]() { ShowForm (asColor); });

        m_context.menus.GetMenuCommands().back()->radio     = true;
        m_context.menus.GetMenuCommands().back()->isChecked = [color, asColor]() { return color == asColor; };
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}
