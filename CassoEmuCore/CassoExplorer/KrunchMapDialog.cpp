#include "Pch.h"

#include "CassoExplorer/KrunchMapDialog.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::GetColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t KrunchMapPanel::GetColor (KrunchMap::Cell cell)
{
    switch (cell)
    {
        case KrunchMap::Cell::System:  return 0xFF808080;
        case KrunchMap::Cell::InPlace: return 0xFF0000A8;
        case KrunchMap::Cell::ToMove:  return 0xFF00A8A8;
        case KrunchMap::Cell::Reading: return 0xFF00C000;
        case KrunchMap::Cell::Writing: return 0xFFE00000;
        default:                       return 0xFFFFFFFF;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring KrunchMapPanel::GetLabel (KrunchMap::Cell cell)
{
    switch (cell)
    {
        case KrunchMap::Cell::System:  return L"Boot blocks and directory";
        case KrunchMap::Cell::InPlace: return L"In place";
        case KrunchMap::Cell::ToMove:  return L"To be moved";
        case KrunchMap::Cell::Reading: return L"Being read";
        case KrunchMap::Cell::Writing: return L"Being written";
        default:                       return L"Free space";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::DescribeFile
//
//  Blocks are listed first to last, as in the Filer's extended listing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring KrunchMapPanel::DescribeFile (const PascalKrunchMove & file, KrunchMap::Cell cell)
{
    std::wstring  name   = TextEncoding::NarrowToWide (file.name);
    std::wstring  blocks = std::format (L"{} {}", file.blocks, (file.blocks == 1) ? L"block" : L"blocks");
    int           last   = file.blocks - 1;



    if (file.to == file.from)
    {
        return std::format (L"{}: {} at {} to {}, which stay in place", name, blocks, file.from, file.from + last);
    }

    if (cell == KrunchMap::Cell::InPlace)
    {
        return std::format (L"{}: {} at {} to {}, moved from {} to {}", name, blocks, file.to, file.to + last, file.from, file.from + last);
    }

    return std::format (L"{}: {} at {} to {}, moving to {} to {}", name, blocks, file.from, file.from + last, file.to, file.to + last);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::SetView
//
//  The block under the pointer isn't reset by a new view, so the file line
//  follows the krunch while the pointer rests on the map.
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapPanel::SetView (View view)
{
    m_view = std::move (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);

    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::GetGrid
//
//  The cells are whole pixels, so the gaps between them are even.
//
////////////////////////////////////////////////////////////////////////////////

KrunchMapPanel::Grid KrunchMapPanel::GetGrid() const
{
    RECT   bounds = GetBounds();
    float  width  = (float) (bounds.right - bounds.left);
    Grid   grid;



    grid.left = (float) bounds.left;
    grid.top  = (float) bounds.top + m_scaler.ToPxf (kHeadingDip) + m_scaler.ToPxf (kSectionGapDip);
    grid.gap  = std::max (1.0f, std::floor (m_scaler.ToPxf (kCellGapDip)));
    grid.cell = std::floor ((width - grid.gap * (KrunchMap::kColumns - 1)) / KrunchMap::kColumns);

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::FindBlockAt
//
//  A point in the gap right of or below a cell is in that cell, so the file
//  line doesn't flicker as the pointer crosses a gap.
//
////////////////////////////////////////////////////////////////////////////////

int KrunchMapPanel::FindBlockAt (POINT pointPx) const
{
    Grid   grid   = GetGrid();
    float  pitch  = grid.cell + grid.gap;
    int    rows   = (KrunchMap::kBlocks + KrunchMap::kColumns - 1) / KrunchMap::kColumns;
    int    column = 0;
    int    row    = 0;
    int    block  = -1;



    if (pitch > 0.0f && pointPx.x >= grid.left && pointPx.y >= grid.top)
    {
        column = (int) ((pointPx.x - grid.left) / pitch);
        row    = (int) ((pointPx.y - grid.top)  / pitch);

        if (column < KrunchMap::kColumns && row < rows && row * KrunchMap::kColumns + column < KrunchMap::kBlocks)
        {
            block = row * KrunchMap::kColumns + column;
        }
    }

    return block;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::IsJoinedRight
//
//  Free space and the boot blocks aren't a file, so each of their cells
//  stays apart.
//
////////////////////////////////////////////////////////////////////////////////

bool KrunchMapPanel::IsJoinedRight (int block) const
{
    size_t  index = (size_t) block;



    if (block % KrunchMap::kColumns == KrunchMap::kColumns - 1 || index + 1 >= m_view.blocks.size())
    {
        return false;
    }

    return m_view.blocks[index].file >= 0 &&
           m_view.blocks[index].file == m_view.blocks[index + 1].file &&
           m_view.blocks[index].cell == m_view.blocks[index + 1].cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool KrunchMapPanel::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hoverBlock = -1;
    }
    else if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Enter)
    {
        m_hoverBlock = FindBlockAt (ev.positionDip);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT       hr      = S_OK;
    RECT          bounds  = GetBounds();
    Grid          grid    = GetGrid();
    float         left    = (float) bounds.left;
    float         width   = (float) (bounds.right - bounds.left);
    int           rows    = (KrunchMap::kBlocks + KrunchMap::kColumns - 1) / KrunchMap::kColumns;
    float         section = m_scaler.ToPxf (kSectionGapDip);
    float         lineH   = m_scaler.ToPxf (kLineDip);
    float         fontPx  = m_scaler.ToPxf (kFontDip);
    float         barPx   = m_scaler.ToPxf (kBarDip);
    float         corner  = barPx * 0.5f;
    float         top     = 0.0f;
    std::wstring  hover   = L"Point at a block to see its file.";
    int           file    = -1;



    if (!IsVisible() || grid.cell < 1.0f)
    {
        return;
    }

    if (m_hoverBlock >= 0 && (size_t) m_hoverBlock < m_view.blocks.size())
    {
        file = m_view.blocks[(size_t) m_hoverBlock].file;
    }

    if (file >= 0 && (size_t) file < m_view.files.size())
    {
        hover = DescribeFile (m_view.files[(size_t) file], m_view.blocks[(size_t) m_hoverBlock].cell);
    }

    hr = text.DrawString (m_view.drive.c_str(), left, (float) bounds.top, width, m_scaler.ToPxf (kHeadingDip),
                          theme.Foreground(), m_scaler.ToPxf (kHeadingFontDip), s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    PaintMap (painter, theme, grid);

    top = grid.top + rows * (grid.cell + grid.gap) + grid.gap;

    hr = text.DrawString (hover.c_str(), left, top, width, lineH,
                          theme.ForegroundMuted(), fontPx, s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    top += lineH + section * 0.5f;

    hr = text.DrawString (m_view.status.c_str(), left, top, width, lineH,
                          theme.Foreground(), fontPx, s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (std::format (L"{}% complete", m_view.percent).c_str(), left, top, width, lineH,
                          theme.ForegroundMuted(), fontPx, s_kpszFont,
                          DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    top += lineH + grid.gap * 2.0f;

    painter.FillRoundedRect    (left, top, width, barPx, corner, theme.BackgroundElevated());
    painter.FillRoundedRect    (left, top, width * (float) std::clamp (m_view.percent, 0, 100) / 100.0f, barPx, corner, theme.Accent());
    painter.OutlineRoundedRect (left, top, width, barPx, corner, std::max (1.0f, m_scaler.ToPxf (1.0f)), theme.Border());

    PaintLegend (painter, text, theme, top + barPx + section);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::PaintMap
//
//  Block 0 is at the top left, a row a kColumns blocks. A map with no
//  blocks yet is drawn as free space. Each run of the file under the
//  pointer is outlined after every cell is filled, so no cell covers an
//  outline.
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapPanel::PaintMap (IDxuiPainter & painter, const IDxuiTheme & theme, const Grid & grid) const
{
    float  pitch   = grid.cell + grid.gap;
    float  outline = m_scaler.ToPxf (kOutlineDip);
    int    file    = -1;
    int    start   = -1;



    for (int block = 0; block < KrunchMap::kBlocks; block++)
    {
        KrunchMap::Block  cell  = ((size_t) block < m_view.blocks.size()) ? m_view.blocks[(size_t) block] : KrunchMap::Block();
        float             x     = grid.left + (block % KrunchMap::kColumns) * pitch;
        float             y     = grid.top  + (block / KrunchMap::kColumns) * pitch;
        float             width = IsJoinedRight (block) ? pitch : grid.cell;

        painter.FillRect (x, y, width, grid.cell, GetColor (cell.cell));
    }

    if (m_hoverBlock >= 0 && (size_t) m_hoverBlock < m_view.blocks.size())
    {
        file = m_view.blocks[(size_t) m_hoverBlock].file;
    }

    for (int block = 0; file >= 0 && block < (int) m_view.blocks.size(); block++)
    {
        if (m_view.blocks[(size_t) block].file != file)
        {
            continue;
        }

        if (start < 0)
        {
            start = block;
        }

        if (!IsJoinedRight (block))
        {
            painter.OutlineRect (grid.left + (start % KrunchMap::kColumns) * pitch - outline * 0.5f,
                                 grid.top  + (start / KrunchMap::kColumns) * pitch - outline * 0.5f,
                                 (block - start) * pitch + grid.cell + outline,
                                 grid.cell + outline,
                                 outline,
                                 theme.Foreground());

            start = -1;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapPanel::PaintLegend
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapPanel::PaintLegend (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const
{
    static constexpr KrunchMap::Cell  s_kOrder[] = { KrunchMap::Cell::InPlace, KrunchMap::Cell::ToMove,  KrunchMap::Cell::Free,
                                                     KrunchMap::Cell::Reading, KrunchMap::Cell::Writing, KrunchMap::Cell::System };



    HRESULT  hr      = S_OK;
    RECT     bounds  = GetBounds();
    float    left    = (float) bounds.left;
    float    column  = (float) (bounds.right - bounds.left) / kLegendColumns;
    float    rowPx   = m_scaler.ToPxf (kLegendRowDip);
    float    swatch  = m_scaler.ToPxf (kSwatchDip);
    float    fontPx  = m_scaler.ToPxf (kFontDip);
    float    edge    = std::max (1.0f, m_scaler.ToPxf (1.0f));



    for (size_t i = 0; i < std::size (s_kOrder); i++)
    {
        float  x = left + (float) (i % kLegendColumns) * column;
        float  y = top  + (float) (i / kLegendColumns) * rowPx;

        painter.FillRect    (x, y + (rowPx - swatch) * 0.5f, swatch, swatch, GetColor (s_kOrder[i]));
        painter.OutlineRect (x, y + (rowPx - swatch) * 0.5f, swatch, swatch, edge, theme.Border());

        hr = text.DrawString (GetLabel (s_kOrder[i]).c_str(), x + swatch * 1.75f, y, column - swatch * 2.0f, rowPx,
                              theme.Foreground(), fontPx, s_kpszFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapDialog::Configure (std::shared_ptr<LinkOperation> operation, const std::wstring & drive, const IDxuiTheme * theme, StopFn stop)
{
    m_operation = std::move (operation);
    m_drive     = drive;
    m_theme     = theme;
    m_stop      = std::move (stop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::DescribeStatus
//
////////////////////////////////////////////////////////////////////////////////

std::wstring KrunchMapDialog::DescribeStatus (const LinkOperation & operation, bool stopping)
{
    LinkOperationState  state  = operation.state;
    bool                ready  = operation.krunchPlanned;
    int                 total  = ready ? KrunchMap::CountFilesToMove (operation.krunchFiles) : 0;
    int                 moved  = operation.krunchMoved;
    int                 moving = ready ? KrunchMap::FindMovingFile (operation.krunchFiles, moved) : -1;
    std::wstring        name;



    switch (state)
    {
        case LinkOperationState::Failed:
            return operation.message.empty() ? std::wstring (L"The krunch failed.") : TextEncoding::NarrowToWide (operation.message);

        case LinkOperationState::Canceled:
            return std::format (L"Stopped between files, with {} of {} moved. Every file is whole.", moved, total);

        case LinkOperationState::Done:
            return (total == 0) ? std::wstring (L"Every file is already in place, so nothing was moved.")
                                : std::format (L"Krunch complete: {} {} moved.", total, (total == 1) ? L"file" : L"files");

        default:
            break;
    }

    if (!ready)
    {
        return L"Reading the directory.";
    }

    if (!operation.krunchTaken)
    {
        return L"Waiting for the Apple to start the krunch.";
    }

    if (stopping)
    {
        return L"Stopping after the file being moved.";
    }

    if (moving >= 0)
    {
        name = TextEncoding::NarrowToWide (operation.krunchFiles[(size_t) moving].name);
    }

    return std::format (L"Moving {}, file {} of {}.", name, std::min (moved + 1, total), total);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::BuildView
//
//  A krunch that fails or stops has the file it was moving whole at its old
//  place, so no file is drawn as being moved.
//
////////////////////////////////////////////////////////////////////////////////

KrunchMapPanel::View KrunchMapDialog::BuildView (const LinkOperation & operation, const std::wstring & drive, bool stopping)
{
    KrunchMapPanel::View  view;
    LinkOperationState    state = operation.state;



    view.drive   = drive;
    view.status  = DescribeStatus (operation, stopping);
    view.percent = operation.progress;

    if (!operation.krunchPlanned)
    {
        return view;
    }

    view.files = operation.krunchFiles;

    if (state == LinkOperationState::Done)
    {
        view.percent = 100;
        view.blocks  = KrunchMap::Build (operation.krunchFiles, true, 100, KrunchMap::CountFilesToMove (operation.krunchFiles));
    }
    else
    {
        bool  moving = operation.krunchTaken && state == LinkOperationState::Running;

        view.blocks = KrunchMap::Build (operation.krunchFiles, moving, view.percent, operation.krunchMoved);
    }

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::OnCreate
//
//  Stop is disabled once the krunch ends, or after it is pressed. The tick
//  is quicker than a dialog's, so the file line follows the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapDialog::OnCreate()
{
    m_panel      = CreateDialogContent<KrunchMapPanel>();
    m_stopButton = AddDialogButton (L"Stop", kStopId);

    AddDialogButton (L"Close", IDCANCEL);

    m_stopButton->SetOnClick ([this]()
    {
        if (m_stop && !m_stopping)
        {
            m_stop();
        }

        m_stopping = true;
        Refresh();
    });

    SetDialogTickIntervalMs (kTickMs);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapDialog::OnDialogTick()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMapDialog::Refresh()
{
    LinkOperationState  state   = m_operation->state;
    bool                running = state == LinkOperationState::Queued || state == LinkOperationState::Running;



    m_panel->SetView (BuildView (*m_operation, m_drive, m_stopping));
    m_stopButton->SetEnabled (running && !m_stopping);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMapDialog::Open
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<KrunchMapDialog> KrunchMapDialog::Open (HWND owner, const IDxuiTheme * theme, std::shared_ptr<LinkOperation> operation, const std::wstring & drive, StopFn stop)
{
    HRESULT                           hr     = S_OK;
    std::unique_ptr<KrunchMapDialog>  dialog = std::make_unique<KrunchMapDialog>();
    DxuiWindow::CreateParams          params;



    dialog->Configure (std::move (operation), drive, theme, std::move (stop));

    params.title                    = L"Krunch disk";
    params.hInstance                = (owner != nullptr) ? (HINSTANCE) GetWindowLongPtrW (owner, GWLP_HINSTANCE) : GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kWidthDip, kHeightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog->Create (params);
    CHRA (hr);

    dialog->SetTheme (theme);
    dialog->ShowModelessDialog (IDCANCEL);

Error:
    return SUCCEEDED (hr) ? std::move (dialog) : nullptr;
}
