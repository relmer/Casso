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
//  again on the next paint. All on the UI thread.
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

    //  A line across the strip where the source stands between its cells:
    //  its offset in cells from the leading edge, and a label for each end of
    //  it. False for no line, when the marked cell is shown instead.
    virtual bool    TryGetPlayhead  (float & outOffset, std::wstring & outTop, std::wstring & outBottom) { (void) outOffset; (void) outTop; (void) outBottom; return false; }

    //  The line dragged to an offset in cells; isFinal once it is let go.
    virtual void    OnPlayheadDragged (float offset, bool isFinal) { (void) offset; (void) isFinal; }

    //  Text for the strip's leading end, such as where it begins, or empty.
    virtual std::wstring  GetLeadingLabel () { return std::wstring(); }

    //  Text for the strip's trailing end, such as where live time is, or
    //  empty; whether it shows in the accent color, its tip, and a click on it.
    virtual std::wstring  GetTrailingLabel        () { return std::wstring(); }
    virtual bool          IsTrailingLabelAccented () { return false; }
    virtual std::wstring  GetTrailingTip          () { return std::wstring(); }
    virtual void          OnTrailingLabelClicked  () {}
};
