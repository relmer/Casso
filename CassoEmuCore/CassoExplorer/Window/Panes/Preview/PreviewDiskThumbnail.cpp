#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewDiskThumbnail.h"
#include "Seams/IDiskPictures.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "Core/AppMessages.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/PlatterCells.h"





static constexpr float     s_kChipPadDip  = 8.0f;
static constexpr float     s_kChipGapDip  = 6.0f;
static constexpr float     s_kChipHighDip = 22.0f;
static constexpr float     s_kChipTextDip = 12.0f;
static constexpr LPCWSTR   s_kpszWaiting  = L"Analyzing";
static constexpr LPCWSTR   s_kpszNote35   = L"Casso does not analyze 3.5\" disks";





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailPlatterView::ThumbnailPlatterView
//
////////////////////////////////////////////////////////////////////////////////

ThumbnailPlatterView::ThumbnailPlatterView (InspectorViewContext & context, const DxuiTheme * const & theme) :
    PlatterView (context),
    m_theme     (theme)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailPlatterView::Paint
//
//  The palette follows a theme change in Explorer (FR-078).
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailPlatterView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_theme != nullptr)
    {
        m_context.palette = DiskInspectorPalette::Resolve (*m_theme);
    }

    PlatterView::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailChips::Paint
//
//  Each chip as the status band draws it, wrapped onto the next row where
//  it does not fit; a bad chip in the bad sector color.
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailChips::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  pad    = m_scaler.ToPxf (s_kChipPadDip);
    float  gap    = m_scaler.ToPxf (s_kChipGapDip);
    float  chipH  = m_scaler.ToPxf (s_kChipHighDip);
    float  rowH   = m_scaler.ToPxf (kRowDip);
    float  textPx = m_scaler.ToPxf (s_kChipTextDip);
    float  left   = static_cast<float> (m_boundsDip.left);
    float  right  = static_cast<float> (m_boundsDip.right);
    float  x      = left;
    float  y      = static_cast<float> (m_boundsDip.top) + (rowH - chipH) / 2.0f;
    float  w      = 0.0f;
    float  h      = 0.0f;



    if (!m_line.empty())
    {
        text.DrawString (m_line.c_str(), left, static_cast<float> (m_boundsDip.top), right - left, rowH, theme.ForegroundMuted(), textPx,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    for (const SummaryChip & chip : m_chips)
    {
        text.MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

        if (x > left && x + w + 2 * pad > right)
        {
            x  = left;
            y += rowH;
        }

        painter.FillRoundedRect    (x, y, w + 2 * pad, chipH, chipH / 2, theme.ButtonIdle());
        painter.OutlineRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, m_scaler.ToPxf (1.0f), chip.isBad ? m_palette.colors.sectorBad : theme.ButtonBorder());
        text.DrawString (chip.text.c_str(), x, y, w + 2 * pad, chipH, chip.isBad ? m_palette.colors.sectorBad : theme.Foreground(), textPx,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        x += w + 2 * pad + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScreen::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailScreen::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr     = S_OK;
    RECT     bounds = GetBounds();



    (void) painter;
    (void) theme;

    if (m_picture != nullptr && m_picture->width > 0 && m_picture->height > 0)
    {
        hr = text.DrawIconBitmap (m_picture->bgraPremul.data(), m_picture->width, m_picture->height, (float) bounds.left, (float) bounds.top,
                                  (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::PreviewDiskThumbnail
//
//  A finished thumbnail is posted to the window, which takes it on the UI
//  thread.
//
////////////////////////////////////////////////////////////////////////////////

PreviewDiskThumbnail::PreviewDiskThumbnail (ExplorerContext & context) :
    m_context (context),
    m_worker  ([this] () { PostMessageW (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerThumbnail, 0, 0); })
{
    m_view.model = &m_model;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Create
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::Create()
{
    m_platter = m_context.window.CreateChild<ThumbnailPlatterView> (m_view, m_context.theme);
    m_chips   = m_context.window.CreateChild<ThumbnailChips> (m_view.palette);
    m_screen  = m_context.window.CreateChild<ThumbnailScreen>();
    m_bootLine = m_context.window.CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_context.window.SetMouseHandler (m_platter, [this] (const DxuiMouseEvent & ev) { return HandlePlatterMouse (ev); });

    m_platter->SetSelectionShown (false);
    m_platter->SetVisible (false);
    m_chips->SetVisible   (false);
    m_screen->SetVisible  (false);
    m_bootLine->SetVisible  (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::SetPath
//
//  Another file starts over with "Analyzing"; an empty path is no
//  thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::SetPath (const std::wstring & path)
{
    if (path == m_path)
    {
        return;
    }

    m_path         = path;
    m_state        = State::Analyzing;
    m_result       = DiskThumbnailResult();
    m_view.hasDisk = false;
    m_view.analysis = nullptr;
    m_chips->SetLine (s_kpszWaiting);

    if (path.empty())
    {
        m_worker.Cancel();
        Hide();
    }
    else
    {
        (void) m_worker.Request (path);
    }

    RefreshScreen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::RefreshScreen
//
//  An image with no picture yet is queued to be booted by the read, and the
//  window is posted a message when its picture is made.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::RefreshScreen()
{
    std::wstring  result;
    bool          found = false;



    m_picture.reset();

    if (!m_path.empty() && m_context.app.pictures != nullptr)
    {
        found = m_context.app.pictures->Find (m_path, m_picture, result);
    }

    m_screen->SetPicture (found ? m_picture : nullptr);
    m_bootLine->SetText (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::TakeResult
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::TakeResult()
{
    if (m_worker.TakeResult (m_result) && m_result.path == m_path)
    {
        ApplyResult();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::ApplyResult
//
//  Each quarter track's ring set from its record's levels, as the
//  inspector sets them, with nothing selected.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::ApplyResult()
{
    const DiskCopy  * copy = (m_result.analysis != nullptr) ? m_result.analysis->copy.get() : nullptr;
    int               qt   = 0;
    int               slot = 0;



    if (!m_result.isOpened || copy == nullptr)
    {
        m_state = State::Failed;
        Hide();
        return;
    }

    if (m_result.is35)
    {
        m_state = State::Note35;
        m_chips->SetLine (s_kpszNote35);
        m_platter->SetVisible (false);
        return;
    }

    m_state         = State::Ready;
    m_view.analysis = m_result.analysis.get();
    m_view.hasDisk  = true;
    m_model.SetDisk (m_result.analysis->mediaId);
    m_model.SetAnalysis (m_result.analysis.get());
    m_model.ClearSelection();

    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        slot = copy->playedSlot[qt];

        if (slot < 0 || slot >= static_cast<int> (m_result.levels.size()) || m_result.levels[slot] == nullptr)
        {
            m_platter->GetRenderer().SetRing (qt, PlatterRingState::Nothing, nullptr);
        }
        else
        {
            m_platter->GetRenderer().SetRing (qt, PlatterRingState::Data, m_result.levels[slot], nullptr, PlatterCells::IsEchoRing (copy->playedSlot, qt),
                                              m_result.edges[slot]);
        }
    }

    m_chips->SetChips (m_result.chips);
    m_platter->SetVisible (m_chips->IsVisible());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::IsBeside
//
//  Whether the screen's picture and the platter fit side by side in the width.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::IsBeside (int widthPx) const
{
    const DxuiDpiScaler  & scaler = m_context.scaler;



    return widthPx >= scaler.ToPx (kScreenDip) + scaler.ToPx (kPlatterDip) + 3 * scaler.ToPx (kGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetHeightPx
//
//  The platter and two rows of chips, or one row for a line in their place.
//  With the screen's picture, two rows for its result as well, and the
//  picture's height where it is above the platter.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewDiskThumbnail::GetHeightPx (int widthPx) const
{
    const DxuiDpiScaler  & scaler = m_context.scaler;
    int                    rows   = scaler.ToPx (static_cast<int> (ThumbnailChips::kRowDip));
    int                    gap    = scaler.ToPx (kGapDip);
    int                    height = scaler.ToPx (kPlatterDip) + 2 * rows + 2 * gap;



    if (!IsShown())
    {
        return 0;
    }

    if (m_state == State::Note35)
    {
        return rows + gap;
    }

    if (HasScreen())
    {
        height += 2 * rows + gap + (IsBeside (widthPx) ? 0 : scaler.ToPx (kScreenDip) * 3 / 4 + gap);
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Layout
//
//  The platter centered across the area's top, and the chips under it. The
//  screen's picture is to the platter's left where both fit, and above it
//  otherwise, and its result is between them and the chips.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::Layout (const RECT & area)
{
    const DxuiDpiScaler  & scaler    = m_context.scaler;
    int                    platter   = scaler.ToPx (kPlatterDip);
    int                    gap       = scaler.ToPx (kGapDip);
    int                    rows      = scaler.ToPx (static_cast<int> (ThumbnailChips::kRowDip));
    int                    width     = static_cast<int> (area.right - area.left);
    bool                   hasScreen = HasScreen();
    bool                   isBeside  = hasScreen && IsBeside (width);
    int                    screenW   = (std::min) (scaler.ToPx (kScreenDip), (std::max) (0, width - 2 * gap));
    int                    screenH   = screenW * 3 / 4;
    int                    group     = platter + (isBeside ? screenW + gap : 0);
    int                    left      = area.left + (std::max) (0, (width - group) / 2);
    int                    top       = area.top + gap;
    int                    chipsTop  = area.top + gap;



    if (!IsShown())
    {
        Hide();
        return;
    }

    if (hasScreen && isBeside)
    {
        m_screen->Layout (RECT { left, top + (platter - screenH) / 2, left + screenW, top + (platter - screenH) / 2 + screenH }, scaler);
        left += screenW + gap;
    }
    else if (hasScreen)
    {
        m_screen->Layout (RECT { area.left + (width - screenW) / 2, top, area.left + (width - screenW) / 2 + screenW, top + screenH }, scaler);
        top += screenH + gap;
    }

    if (m_state != State::Note35)
    {
        m_platter->Layout (RECT { left, top, left + platter, top + platter }, scaler);
        chipsTop = top + platter + gap;
    }

    if (hasScreen)
    {
        m_bootLine->Layout (RECT { area.left + gap, chipsTop, area.right - gap, chipsTop + 2 * rows }, scaler);
        chipsTop += 2 * rows + gap;
    }

    m_chips->Layout (RECT { area.left + gap, chipsTop, area.right - gap, chipsTop + 2 * rows }, scaler);
    m_chips->SetVisible   (true);
    m_platter->SetVisible (m_state == State::Ready);
    m_screen->SetVisible  (hasScreen);
    m_bootLine->SetVisible  (hasScreen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::Hide()
{
    m_platter->SetVisible (false);
    m_chips->SetVisible   (false);
    m_screen->SetVisible  (false);
    m_bootLine->SetVisible  (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::IsOverPlatter
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::IsOverPlatter (POINT pointPx) const
{
    RECT  bounds = m_platter->GetBounds();



    return m_state == State::Ready && m_platter->IsVisible() && PtInRect (&bounds, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::HandlePlatterMouse
//
//  A click on the small platter opens the inspector.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::HandlePlatterMouse (const DxuiMouseEvent & ev)
{
    if (!IsOverPlatter (ev.positionDip))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left)
    {
        m_context.commandManager.InspectDiskImages ({ m_path });
    }

    return true;
}
