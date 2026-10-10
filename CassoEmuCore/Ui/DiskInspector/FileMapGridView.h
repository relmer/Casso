#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView
//
//  The File map tab's grid (FR-089): a row per track, 0 to 34, and a cell
//  per DOS 3.3 logical sector, ProDOS or Pascal block or CP/M sector, each
//  in its role's color with the role's letter once it fits, and its result
//  marked apart from the color: a cross for bad, an outline for missing, a
//  dot for not checked (FR-086). The selected file's sectors carry their
//  numbers in file order, and the selected sector is outlined. A click
//  chooses a cell; a disk that is not mapped shows why above each sector's
//  result by track and physical sector (FR-094).
//
////////////////////////////////////////////////////////////////////////////////

class FileMapGridView : public InspectorView
{
public:
    using ChooseFn = std::function<void (int cell)>;

    explicit FileMapGridView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    void  SetMap         (const FileMap * map)    { m_map = map; }
    void  SetSelected    (int cell, int file)     { m_selectedCell = cell; m_selectedFile = file; }
    void  SetOnChoose    (ChooseFn choose)        { m_choose = std::move (choose); }

    //  The grid's height for a width, so the file list gets the rest.
    int   GetHeightFor   (int widthPx) const;

private:
    struct Grid
    {
        float  left    = 0;
        float  top     = 0;
        float  cellW   = 1;
        float  rowH    = 1;
        float  labelW  = 0;
    };

    Grid  MakeGrid  () const;
    int   HitTest   (POINT pointPx) const;
    void  PaintMap  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  PaintCell (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, int cell, int number);

    const FileMap *  m_map          = nullptr;
    int              m_selectedCell = -1;
    int              m_selectedFile = -1;
    ChooseFn         m_choose;
};
