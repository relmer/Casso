#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewDiskThumbnail.h"
#include "Seams/IDiskPictures.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "Core/AppMessages.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Seams/Win32DiskFileIo.h"
#include "Ui/DiskInspector/RepairDialog.h"
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
//  PreviewDiskThumbnail::FormatVerdict
//
//  "File system damaged." or "File system suspect.", and the finding as a
//  sentence on the next line; nothing for OK or a disk with no standard
//  file system.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDiskThumbnail::FormatVerdict (FileSystemVerdict verdict, const std::wstring & finding)
{
    std::wstring  text;
    std::wstring  sentence = finding;



    if (!sentence.empty())
    {
        sentence[0] = static_cast<wchar_t> (towupper (sentence[0]));
    }

    if (!sentence.empty() && !sentence.ends_with (L'.'))
    {
        sentence += L".";
    }

    if (verdict == FileSystemVerdict::Bad)
    {
        text = L"File system damaged." + (sentence.empty() ? std::wstring() : L"\n" + sentence);
    }
    else if (verdict == FileSystemVerdict::Suspect)
    {
        text = L"File system suspect." + (sentence.empty() ? std::wstring() : L"\n" + sentence);
    }

    return text;
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
    m_header   = m_context.window.CreateChild<DxuiScrollPanel>();
    m_platter  = m_header->CreateChild<ThumbnailPlatterView> (m_view, m_context.theme);
    m_chips    = m_header->CreateChild<ThumbnailChips> (m_view.palette);
    m_screen   = m_header->CreateChild<ThumbnailScreen>();
    m_bootLine = m_header->CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_verdict  = m_header->CreateChild<DxuiInfoBanner>();

    m_header->SetScrollbarShown  (false);
    m_verdict->SetActionWidthDip (static_cast<float> (kRepairDip));
    m_verdict->SetStackBelowDip  (static_cast<float> (kStackDip));
    m_verdict->SetOnAction       ([this] (size_t) { RepairImage(); });

    m_context.window.SetMouseHandler (m_header, [this] (const DxuiMouseEvent & ev) { return HandleHeaderMouse (ev); });

    m_platter->SetSelectionShown (false);
    Hide();
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
    m_isVerdictShown = false;
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
    const DiskCopy  * copy    = (m_result.analysis != nullptr) ? m_result.analysis->copy.get() : nullptr;
    const Finding   * finding = nullptr;
    int               qt      = 0;
    int               slot    = 0;
    std::wstring      text;



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

    finding          = FileSystemCheck::GetVerdictFinding (*m_result.analysis);
    text             = FormatVerdict (m_result.analysis->verdict, finding != nullptr ? FindingFormatter::Format (*finding) : std::wstring());
    m_isVerdictShown = !text.empty();

    m_verdict->SetText     (text);
    m_verdict->SetSeverity ((m_result.analysis->verdict == FileSystemVerdict::Bad) ? DxuiInfoBanner::Severity::Error : DxuiInfoBanner::Severity::Warning);
    m_verdict->SetActions  (m_result.canRepair ? vector<std::wstring> { L"Repair disk image" } : vector<std::wstring>());
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

    return height + GetVerdictHeightPx (widthPx);
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

int PreviewDiskThumbnail::Layout (const RECT & area, int scrollPx, int bottomPx)
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
    int                    shown     = 0;



    if (!IsShown())
    {
        Hide();
        return area.top;
    }

    if (HasVerdict())
    {
        PlaceVerdict (area, top, GetVerdictHeightPx (width) - gap);
        top     += GetVerdictHeightPx (width);
        chipsTop = top;
    }

    if (hasScreen && isBeside)
    {
        m_header->PlaceChild (*m_screen, RECT { left, top + (platter - screenH) / 2, left + screenW, top + (platter - screenH) / 2 + screenH });
        left += screenW + gap;
    }
    else if (hasScreen)
    {
        m_header->PlaceChild (*m_screen, RECT { area.left + (width - screenW) / 2, top, area.left + (width - screenW) / 2 + screenW, top + screenH });
        top += screenH + gap;
    }

    if (m_state != State::Note35)
    {
        m_header->PlaceChild (*m_platter, RECT { left, top, left + platter, top + platter });
        chipsTop = top + platter + gap;
    }

    if (hasScreen)
    {
        m_header->PlaceChild (*m_bootLine, RECT { area.left + gap, chipsTop, area.right - gap, chipsTop + 2 * rows });
        chipsTop += 2 * rows + gap;
    }

    m_header->PlaceChild (*m_chips, RECT { area.left + gap, chipsTop, area.right - gap, chipsTop + 2 * rows });

    m_header->SetVisible  (true);
    m_chips->SetVisible   (true);
    m_platter->SetVisible (m_state == State::Ready);
    m_screen->SetVisible  (hasScreen);
    m_bootLine->SetVisible  (hasScreen);
    m_verdict->SetVisible (HasVerdict());

    //  Laid out whole to measure it, then in the part not scrolled away.
    m_header->Layout (area, scaler);
    shown = std::clamp ((std::min) (static_cast<int> (area.bottom - area.top), m_header->GetContentHeightPx()) - scrollPx, 0, (std::max) (0, bottomPx - static_cast<int> (area.top)));

    m_header->Layout         (RECT { area.left, area.top, area.right, area.top + shown }, scaler);
    m_header->SetScrollPosPx (scrollPx);

    return area.top + shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::Hide()
{
    m_header->SetVisible  (false);
    m_platter->SetVisible (false);
    m_chips->SetVisible   (false);
    m_screen->SetVisible  (false);
    m_bootLine->SetVisible  (false);
    m_verdict->SetVisible   (false);
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
//  PreviewDiskThumbnail::HandleHeaderMouse
//
//  The platter's and the picture's clicks, then the infobar's action. The
//  wheel is the pane's, so the header doesn't scroll by itself.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::HandleHeaderMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        return true;
    }

    return HandlePlatterMouse (ev) || HandleScreenMouse (ev) || m_header->OnMouse (ev);
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





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetVerdictHeightPx
//
//  The infobar as tall as its text wrapped to the pane's width, measured
//  where the window has a text renderer, and a gap; nothing for none.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewDiskThumbnail::GetVerdictHeightPx (int widthPx) const
{
    const DxuiDpiScaler  & scaler = m_context.scaler;
    IDxuiTextRenderer    * text   = m_context.window.GetTextRenderer();
    int                    gap    = scaler.ToPx (kGapDip);
    float                  width  = static_cast<float> ((std::max) (0, widthPx - 2 * gap));
    float                  height = 0.0f;



    if (!HasVerdict())
    {
        return 0;
    }

    height = (text != nullptr) ? m_verdict->GetMeasuredHeightPx (*text, width, scaler) : m_verdict->GetPreferredHeightPx (width, scaler);

    return static_cast<int> (std::ceil (height)) + gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::PlaceVerdict
//
//  The infobar across the area's top. Repair disk image is at its right, or
//  under its text in a narrow pane, when the volume can be repaired.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::PlaceVerdict (const RECT & area, int top, int height)
{
    int  gap = m_context.scaler.ToPx (kGapDip);



    m_header->PlaceChild (*m_verdict, RECT { area.left + gap, top, area.right - gap, top + height });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::RepairImage
//
//  The confirmation from the file as saved, with every change checked, and
//  the copy written beside the image and selected. The status bar has the
//  copy's name or the reason it wasn't written.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::RepairImage()
{
    HRESULT          hr     = S_OK;
    Win32DiskFileIo  fileIo;
    std::string      written;
    std::wstring     note;



    hr = RepairDialog::Run (m_context.window.GetHwnd(), m_context.theme, fileIo, TextEncoding::WideToUtf8 (m_path), {}, written, note);

    if (!note.empty())
    {
        m_context.statusBar.SetActivity (note);
    }

    if (SUCCEEDED (hr) && !written.empty())
    {
        m_context.tabManager.SelectInFolder (TextEncoding::Utf8ToWide (written));
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::HandleScreenMouse
//
//  The booted screen's picture has a menu with Copy, which puts the picture
//  on the clipboard.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::HandleScreenMouse (const DxuiMouseEvent & ev)
{
    RECT                                  bounds  = m_screen->GetBounds();
    std::shared_ptr<const DxuiIconImage>  picture = m_picture;
    std::vector<DxuiPopupMenuItem>        items;



    if (!HasScreen() || !m_screen->IsVisible() || PtInRect (&bounds, ev.positionDip) == FALSE)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
        m_context.input.ReleasePressed();
        m_context.menus.GetMenuCommands().clear();
        m_context.menus.AddMenuCommand (items, L"&Copy", [this, picture]() { ExplorerPreviewOutput::CopyPicture (m_context, *picture); });
        DxuiContextMenu::Show (*m_context.window.GetPopupHost(), ev.positionDip.x, ev.positionDip.y, std::move (items));
    }

    return true;
}
