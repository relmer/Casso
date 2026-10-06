#pragma once

#include "Pch.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiImageStripSource
//
//  What a DxuiImageStrip shows. The strip says how many cells it has room
//  for and how big each is, in pixels, every time it is laid out; the source
//  answers with a picture per cell, at that size or any other (the strip
//  scales it), and a full-size picture for the cell under the pointer. A
//  picture not ready yet is null: the strip draws an empty cell, and asks
//  again on the next paint. A cell's preview and labels are of the same
//  snapshot as the picture the cell last answered with, so the three always
//  agree; the labels and a click for a point along the strip are that
//  point's own, by its offset in cells. All on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiImageStripSource
{
public:
    using Image = std::shared_ptr<const DxuiIconImage>;

    virtual         ~IDxuiImageStripSource() = default;

    virtual void    SetCellLayout   (int count, SIZE cellPx) = 0;
    virtual Image   GetCellImage    (int index)              = 0;
    virtual Image   GetPreviewImage (int index)              = 0;
    virtual void    OnCellClicked   (int index)              = 0;

    //  The cell to mark as where the source stands, or -1 for none.
    virtual int     GetMarkedCell   ()                       { return -1; }

    //  The cell under the pointer, or -1 once it leaves the strip, so the
    //  source can get the pictures around it ready.
    virtual void    SetHoveredCell  (int index)              { (void) index; }

    //  A label above and below a cell under the pointer, such as when its
    //  picture was taken. False for none.
    virtual bool    TryGetCellLabels (int index, std::wstring & outTop, std::wstring & outBottom) { (void) index; (void) outTop; (void) outBottom; return false; }

    //  The labels for the point under the pointer, at an offset in cells
    //  from the leading edge within cell `index`, such as the time there; by
    //  default, the cell's own.
    virtual bool    TryGetLabelsAt  (int index, float offset, std::wstring & outTop, std::wstring & outBottom) { (void) offset; return TryGetCellLabels (index, outTop, outBottom); }

    //  A click at an offset in cells from the leading edge, within cell
    //  `index`; by default, a click on the cell.
    virtual void    OnStripClicked  (int index, float offset)  { (void) offset; OnCellClicked (index); }

    //  A line across the strip where the source stands between its cells:
    //  its offset in cells from the leading edge, and a label for each end of
    //  it. False for no line, when the marked cell is shown instead.
    virtual bool    TryGetPlayhead  (float & outOffset, std::wstring & outTop, std::wstring & outBottom) { (void) outOffset; (void) outTop; (void) outBottom; return false; }

    //  The line dragged to an offset in cells; isFinal once it is let go.
    virtual void    OnPlayheadDragged (float offset, bool isFinal) { (void) offset; (void) isFinal; }

    //  Text for the strip's leading end, such as where it begins, or empty;
    //  its tip, and a click on it.
    virtual std::wstring  GetLeadingLabel       () { return std::wstring(); }
    virtual std::wstring  GetLeadingTip         () { return std::wstring(); }
    virtual void          OnLeadingLabelClicked () {}

    //  Text for the strip's trailing end, such as where live time is, or
    //  empty; whether it shows in the accent color, whether it is a button
    //  now (a label that is not shows no hover or pressed chrome and takes
    //  no click, but keeps its tip), its tip, and a click on it.
    virtual std::wstring  GetTrailingLabel         () { return std::wstring(); }
    virtual bool          IsTrailingLabelAccented  () { return false; }
    virtual bool          IsTrailingLabelClickable () { return true; }
    virtual std::wstring  GetTrailingTip           () { return std::wstring(); }
    virtual void          OnTrailingLabelClicked   () {}
};
