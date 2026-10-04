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
};
