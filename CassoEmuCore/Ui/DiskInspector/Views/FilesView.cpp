#include "Pch.h"

#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/FileMapGridView.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorTableView.h"





static constexpr int  s_kMapCheckDip     = 190;
static constexpr int  s_kMapButtonDip    = 30;
static constexpr int  s_kMapStepLabelDip = 150;
static constexpr int  s_kMapVolumeDip    = 150;
static constexpr int  s_kPickerPadDip    = 8;
static constexpr int  s_kChevronDip      = 5;





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::FilesView
//
////////////////////////////////////////////////////////////////////////////////

FilesView::FilesView (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::Create
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::Create()
{
    DxuiPanel &  parent = m_host.GetControlParent();



    m_mapScroll    = parent.CreateChild<DxuiScrollPanel>();
    m_mapGrid      = m_mapScroll->CreateChild<FileMapGridView>    (m_session.context);
    m_fileList     = m_mapScroll->CreateChild<InspectorTableView> (m_session.context);
    m_mapVolumes   = parent.CreateChild<DxuiTabStrip>();
    m_mapDeleted   = parent.CreateChild<DxuiCheckbox> (L"Show deleted files");
    m_mapBadOnly   = parent.CreateChild<DxuiCheckbox> (L"Files touching bad sectors");
    m_mapPrev      = parent.CreateChild<DxuiButton> (L"<");
    m_mapNext      = parent.CreateChild<DxuiButton> (L">");
    m_mapStepLabel = parent.CreateChild<DxuiLabel> (L"Previous/next sector in", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_filePicker   = parent.CreateChild<DxuiButton> (L"");
    m_mapCopy      = parent.CreateChild<DxuiButton> (L"Copy map");

    m_mapGrid->SetOnChoose    ([this] (int cell) { SelectMapCell (cell); });
    m_mapGrid->SetOnZoom      ([this] () { m_host.Relayout(); });
    m_fileList->SetOnSelect   ([this] (const TableRow & row) { ChooseFile (row.finding); });
    m_fileList->SetOnSort     ([this] (int column) { m_fileSortDescending = (column == m_fileSortColumn) && !m_fileSortDescending; m_fileSortColumn = column; RefreshFileList(); });
    m_mapVolumes->SetOnChange ([this] (int index) { m_session.mapIndex = index; m_session.mapFile = -1; RefreshFileList(); });
    m_mapDeleted->SetOnChange ([this] (bool) { RefreshFileList(); });
    m_mapBadOnly->SetOnChange ([this] (bool) { RefreshFileList(); });
    m_mapPrev->SetOnClick     ([this] () { StepFileSector (-1); });
    m_mapNext->SetOnClick     ([this] () { StepFileSector (1); });
    m_filePicker->SetOnClick  ([this] () { OpenFilePicker (!m_isPickerOpen); });
    m_mapCopy->SetOnClick     ([this] () { CopyFileMap(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FilesView::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    const std::pair<const IDxuiControl *, LPCWSTR>  tips[] =
    {
        { m_mapPrev,    L"Select the previous sector in the selected file" },
        { m_filePicker, L"Choose the file whose sectors the map marks" },
        { m_mapNext,    L"Select the next sector in the selected file" },
    };



    std::wstring  tip;



    for (const auto & [control, text] : tips)
    {
        RECT  bounds = control->GetBounds();

        if (tip.empty() && control->IsVisible() && PtInRect (&bounds, pointPx))
        {
            tip         = text;
            outAnchorPx = bounds;
        }
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::GetGridTip
//
//  The grid's tooltip, only over its part in the scrolling view.
//
////////////////////////////////////////////////////////////////////////////////

bool FilesView::GetGridTip (POINT pointPx, std::wstring & outTip, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    RECT  viewport = m_mapScroll->GetViewportPx();
    RECT  bounds   = m_mapGrid->GetBounds();
    bool  isTip    = m_mapGrid->IsVisible() && PtInRect (&viewport, pointPx) && PtInRect (&bounds, pointPx) && m_mapGrid->GetTooltip (pointPx, outTip, outAnchorPx);



    if (isTip)
    {
        m_mapGrid->GetTooltipSwatches (pointPx, outSwatches);
    }

    return isTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::IsListAt
//
////////////////////////////////////////////////////////////////////////////////

bool FilesView::IsListAt (POINT pointPx) const
{
    RECT  bounds = m_fileList->GetBounds();



    return m_fileList->IsVisible() && PtInRect (&bounds, pointPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::ResetZoom
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::ResetZoom()
{
    m_mapGrid->ResetZoom();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::PaintOver
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::PaintOver (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_filePicker->IsVisible())
    {
        PaintPicker (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::GetPrefs (DiskInspectorPrefs & inOut) const
{
    inOut.isShowDeleted = m_mapDeleted->IsChecked();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::ApplyPrefs
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::ApplyPrefs (const DiskInspectorPrefs & prefs)
{
    m_mapDeleted->SetChecked (prefs.isShowDeleted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::Layout
//
//  Two rows of controls, the grid, then the file list; on a disk that is not
//  mapped, the grid fills the whole area.
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    const FileMap *            map     = m_session.GetFileMap();
    int                        row     = scaler.ToPx (InspectorMetrics::kRowDip);
    int                        margin  = scaler.ToPx (InspectorMetrics::kMarginDip);
    int                        check   = scaler.ToPx (s_kMapCheckDip);
    int                        button  = scaler.ToPx (s_kMapButtonDip);
    int                        volume  = scaler.ToPx (s_kMapVolumeDip);
    int                        label   = scaler.ToPx (s_kMapStepLabelDip);
    int                        picker  = GetPickerWidthPx (scaler);
    int                        y       = area.top;
    int                        width   = 0;
    int                        gridH   = 0;
    bool                       isList  = map != nullptr && map->notMapped == NotMappedReason::None;
    vector<DxuiTabStrip::Tab>  tabs;



    //  The step buttons, their label and the file picker, the volume tabs,
    //  and "Copy map" at the right, on one row.
    m_mapPrev->Layout      ({ area.left,          y, area.left + button,     y + row }, scaler);
    m_mapNext->Layout      ({ area.left + button, y, area.left + 2 * button, y + row }, scaler);
    m_mapStepLabel->Layout ({ area.left + 2 * button + margin, y, area.left + 2 * button + margin + label, y + row }, scaler);
    m_filePicker->Layout   ({ area.left + 2 * button + margin + label, y, area.left + 2 * button + margin + label + picker, y + row }, scaler);
    m_mapCopy->Layout      ({ area.right - scaler.ToPx (InspectorMetrics::kExportButtonDip), y, area.right, y + row }, scaler);

    for (size_t v = 0; v < m_session.analysis.fileMaps.size() && m_session.analysis.fileMaps.size() > 1; v++)
    {
        int  left = area.left + 2 * button + 2 * margin + label + picker + static_cast<int> (v) * volume;

        tabs.push_back ({ { left, y, left + volume, y + row }, FileMapText::FormatVolume (m_session.analysis.fileMaps[v]) });
    }

    m_mapVolumes->SetTabs     (std::move (tabs));
    m_mapVolumes->SetSelected (m_session.mapIndex);
    m_mapVolumes->Layout      ({ area.left + 2 * button + 2 * margin + label + picker, y, area.right - scaler.ToPx (InspectorMetrics::kExportButtonDip) - margin, y + row }, scaler);
    y += row + margin / 2;

    //  Open, the picker has the list's filters and the file list in the rest
    //  of the tab; closed, the tab has the grid, its rows at least as tall as its
    //  labels, with a scrollbar at the right when it does not fit.
    width = area.right - area.left;

    if (isList && m_isPickerOpen)
    {
        m_mapDeleted->Layout ({ area.left,         y, area.left + check,     y + row }, scaler);
        m_mapBadOnly->Layout ({ area.left + check, y, area.left + 2 * check, y + row }, scaler);
        y += row + margin / 2;

        m_mapScroll->PlaceChild (*m_fileList, { area.left, y, area.right, area.bottom });
        m_mapScroll->PlaceChild (*m_mapGrid,  { area.left, y, area.right, y });
    }
    else if (m_mapGrid->IsZoomed())
    {
        //  Zoomed, the grid fills the view and pans in it.
        m_mapScroll->PlaceChild (*m_mapGrid,  { area.left, y, area.right, area.bottom });
        m_mapScroll->PlaceChild (*m_fileList, { area.left, area.bottom, area.right, area.bottom });
    }
    else
    {
        gridH = isList ? m_mapGrid->GetHeightFor (width) : area.bottom - y;

        if (isList && gridH > area.bottom - y)
        {
            width -= scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip) + margin / 2;
            gridH  = m_mapGrid->GetHeightFor (width);
        }

        m_mapScroll->PlaceChild (*m_mapGrid,  { area.left, y, area.left + width, y + gridH });
        m_mapScroll->PlaceChild (*m_fileList, { area.left, y + gridH, area.left + width, y + gridH });
    }

    m_mapScroll->SetLineStepPx (scaler.ToPx (InspectorMetrics::kRowDip));
    m_mapScroll->Layout        ({ area.left, y, area.right, area.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::Show
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::Show (bool isShown)
{
    const FileMap *  map    = m_session.GetFileMap();
    bool             isList = isShown && map != nullptr && map->notMapped == NotMappedReason::None;



    bool             isOpen = isList && m_isPickerOpen;



    m_mapScroll->SetVisible  (isShown);
    m_mapGrid->SetVisible    (isShown && !isOpen);
    m_fileList->SetVisible   (isOpen);
    m_mapDeleted->SetVisible (isOpen);
    m_mapBadOnly->SetVisible (isOpen);
    m_filePicker->SetVisible (isList);

    //  The step buttons are enabled only with a file chosen and the picker
    //  closed.
    m_mapPrev->SetEnabled (!m_isPickerOpen && m_session.mapFile >= 0);
    m_mapNext->SetEnabled (!m_isPickerOpen && m_session.mapFile >= 0);
    m_mapPrev->SetVisible    (isList);
    m_mapNext->SetVisible    (isList);
    m_mapStepLabel->SetVisible (isList);
    m_mapCopy->SetVisible    (isList);
    m_mapVolumes->SetVisible (isShown && m_session.analysis.fileMaps.size() > 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::OpenFilePicker
//
//  The file picker opens to the file list and closes to the chosen file's
//  map, as a drop-down list does.
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::OpenFilePicker (bool isOpen)
{
    m_isPickerOpen = isOpen;
    Show (m_session.isDiskGroup && m_session.diskTab == kTabFileMap);
    m_host.Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::PaintPicker
//
//  The picker's label at its left and a chevron at its right, as in a
//  drop-down list: pointing down while the picker is closed and up while it
//  is open. A label of a new width causes a relayout of the row, so the
//  picker is as wide as its label and chevron.
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::PaintPicker (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   b      = m_filePicker->GetBounds();
    float  half   = m_session.scaler.ToPxf (static_cast<float> (s_kChevronDip));
    float  cx     = static_cast<float> (b.right) - 3 * half;
    float  cy     = (b.top + b.bottom) / 2.0f;
    float  tip    = m_isPickerOpen ? -half / 2 : half / 2;
    float  textPx = m_session.scaler.ToPxf (DxuiTheme::kBodySizeDip);
    float  w      = 0;
    float  h      = 0;



    text.MeasureString (m_pickerLabel.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

    if (std::abs (w - m_pickerTextPx) > 0.5f)
    {
        m_pickerTextPx          = w;
        m_session.isLayoutStale = true;
    }

    text.DrawString (m_pickerLabel.c_str(), static_cast<float> (b.left) + m_session.scaler.ToPxf (static_cast<float> (s_kPickerPadDip)), static_cast<float> (b.top),
                     static_cast<float> (b.right - b.left), static_cast<float> (b.bottom - b.top), theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

    painter.DrawLine (cx - half, cy - tip, cx,        cy + tip, 1.5f, theme.Foreground());
    painter.DrawLine (cx,        cy + tip, cx + half, cy - tip, 1.5f, theme.Foreground());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::SetPickerLabel
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::SetPickerLabel (const std::wstring & label)
{
    m_pickerLabel           = label;
    m_session.isLayoutStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::GetPickerWidthPx
//
//  The label's measured width, or before it has been drawn a guess from its
//  length, with padding and room for the chevron.
//
////////////////////////////////////////////////////////////////////////////////

int FilesView::GetPickerWidthPx (const DxuiDpiScaler & scaler) const
{
    float  label = (m_pickerTextPx > 0) ? m_pickerTextPx : static_cast<float> (m_pickerLabel.size()) * scaler.ToPxf (static_cast<float> (InspectorMetrics::kCharWidthDip));



    return static_cast<int> (std::ceil (label)) + scaler.ToPx (2 * s_kPickerPadDip + 4 * s_kChevronDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::RefreshFileList
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::RefreshFileList()
{
    const FileMap *  map = m_session.GetFileMap();



    m_session.mapFile = (map != nullptr && m_session.mapFile < static_cast<int> (map->files.size())) ? m_session.mapFile : -1;
    m_mapGrid->SetMap (map);

    if (map != nullptr)
    {
        m_fileList->SetColumns (FileMapText::GetFileColumns (*map));
        m_fileList->SetRows    (FileMapText::BuildFileRows (*map, m_mapDeleted->IsChecked(), m_mapBadOnly->IsChecked(), m_fileSortColumn, m_fileSortDescending));
        m_fileList->SetCaption (map->isCatalogComplete ? L"" : L"The catalog is incomplete: " + FileMapText::FormatCell (*map, map->unreadableCell) + L" could not be read");
    }
    else
    {
        m_fileList->SetRows (vector<TableRow>());
    }

    m_session.context.fileMap      = map;
    m_session.context.selectedFile = m_session.mapFile;
    SetPickerLabel ((map != nullptr && m_session.mapFile >= 0) ? map->files[m_session.mapFile].path : std::wstring (L"Choose a file"));
    Show (m_session.isDiskGroup && m_session.diskTab == kTabFileMap);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::ChooseFile
//
//  A file chosen in the list: its sectors marked, and its first selected
//  in every view (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::ChooseFile (int file)
{
    const FileMap *  map = m_session.GetFileMap();



    m_session.mapFile              = file;
    m_session.context.selectedFile = file;

    //  A chosen file closes the picker, which then has the file's name.
    SetPickerLabel ((map != nullptr && file >= 0) ? map->files[file].path : std::wstring (L"Choose a file"));
    OpenFilePicker (false);

    for (const FilePlace & place : (map != nullptr && file >= 0) ? map->files[file].sectors : vector<FilePlace>())
    {
        if (place.cell >= 0)
        {
            SelectMapCell (place.cell);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::SelectMapCell
//
//  The physical sector that contains a cell, or the first half of a block, on
//  its whole track, from the first field with that number and an address
//  field that reads (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::SelectMapCell (int cell)
{
    const FileMap *        map      = m_session.GetFileMap();
    int                    qt       = 0;
    int                    physical = 0;
    int                    found    = -1;
    const TrackAnalysis *  track    = nullptr;



    if (map != nullptr && cell >= 0)
    {
        qt       = map->GetTrack (cell) * DiskImage::kQuarterTracksPerWholeTrack;
        physical = map->GetPhysical (cell, 0);

        m_session.model.SelectQuarterTrack (qt);
        track = m_session.model.GetTrack();

        for (size_t s = 0; track != nullptr && found < 0 && s < track->sectors.size(); s++)
        {
            const AnalyzedSector &  sector = track->sectors[s];

            found = (sector.sector == physical && (sector.isAddressGood || !sector.isAddressCheck)) ? static_cast<int> (s) : -1;
        }

        if (found >= 0)
        {
            m_session.model.SelectSector (qt, found);
        }

        m_host.OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::StepFileSector
//
//  "Next sector in file" and "Previous sector in file", across tracks.
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::StepFileSector (int step)
{
    const FileMap *  map     = m_session.GetFileMap();
    vector<int>      cells;
    int              current = GetSelectedMapCell();
    int              at      = -1;



    for (const FilePlace & place : (map != nullptr && m_session.mapFile >= 0) ? map->files[m_session.mapFile].sectors : vector<FilePlace>())
    {
        if (place.cell >= 0)
        {
            at = (place.cell == current && at < 0) ? static_cast<int> (cells.size()) : at;
            cells.push_back (place.cell);
        }
    }

    if (!cells.empty())
    {
        SelectMapCell (cells[std::clamp (at < 0 ? 0 : at + step, 0, static_cast<int> (cells.size()) - 1)]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::GetSelectedMapCell
//
////////////////////////////////////////////////////////////////////////////////

int FilesView::GetSelectedMapCell() const
{
    const FileMap *         map    = m_session.GetFileMap();
    const AnalyzedSector *  sector = m_session.model.GetSector();
    int                     qt     = m_session.model.GetQuarterTrack();
    int                     half   = 0;



    return (map != nullptr && sector != nullptr && qt % DiskImage::kQuarterTracksPerWholeTrack == 0)
               ? map->GetCellOf (qt / DiskImage::kQuarterTracksPerWholeTrack, sector->sector, half) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::SyncSelection
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::SyncSelection()
{
    int   cell   = GetSelectedMapCell();
    RECT  target = {};



    m_mapGrid->SetSelected (cell, m_session.mapFile);

    //  A sector selected anywhere is scrolled to the middle of the grid's
    //  view, once, or panned there in a zoomed grid.
    if (cell >= 0 && cell != m_revealedCell && m_mapGrid->GetCellRect (cell, target) && m_mapGrid->IsZoomed())
    {
        RECT  view = m_mapGrid->GetBounds();

        m_mapGrid->ShiftPan ((target.left + target.right) / 2 - (view.left + view.right) / 2, (target.top + target.bottom) / 2 - (view.top + view.bottom) / 2);
    }
    else if (cell >= 0 && cell != m_revealedCell && m_mapGrid->GetCellRect (cell, target))
    {
        RECT  view = m_mapScroll->GetViewportPx();

        m_mapScroll->SetScrollPosPx (m_mapScroll->GetScrollPosPx() + (target.top + target.bottom) / 2 - (view.top + view.bottom) / 2);
    }

    m_revealedCell = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::IsOverGrid
//
//  Whether the point is on the File map grid in view: the File map tab
//  chosen, the picker closed, and the point in the grid's viewport.
//
////////////////////////////////////////////////////////////////////////////////

bool FilesView::IsOverGrid (POINT pointPx) const
{
    RECT  grid     = m_mapGrid->GetBounds();
    RECT  viewport = m_mapScroll->GetViewportPx();



    return m_mapGrid->IsVisible() && !m_isPickerOpen && PtInRect (&viewport, pointPx) != FALSE && PtInRect (&grid, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::ZoomGrid
//
//  The grid zoomed about the point by the wheel's notches. Entering or
//  leaving the zoom changes the grid's bounds, from its whole height in the
//  scrolling view to the view itself, so the pan shifts by as much as the
//  bounds moved and the point stays under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::ZoomGrid (float notches, POINT pointPx)
{
    RECT  before   = m_mapGrid->GetBounds();
    RECT  after    = {};
    bool  isZoomed = m_mapGrid->IsZoomed();



    m_mapGrid->ZoomAbout (std::pow (InspectorMetrics::kZoomStep, notches), pointPx);

    if (m_mapGrid->IsZoomed() != isZoomed)
    {
        m_mapScroll->SetScrollPosPx (0);
        m_host.Relayout();

        after = m_mapGrid->GetBounds();
        m_mapGrid->ShiftPan (after.left - before.left, after.top - before.top);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView::CopyFileMap
//
//  "Copy map" (FR-095).
//
////////////////////////////////////////////////////////////////////////////////

void FilesView::CopyFileMap()
{
    const FileMap *  map = m_session.GetFileMap();



    if (map != nullptr)
    {
        (void) m_session.clipboard.SetText (m_host.GetOwnerHwnd(), FileMapText::FormatMap (*map));
    }
}
