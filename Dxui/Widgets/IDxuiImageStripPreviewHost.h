#pragma once

#include "Pch.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiImageStripPreviewHost
//
//  The strip a DxuiImageStripPreview opens beside: where its popup goes, in
//  screen pixels, for a popup of a given size, and which side it opens on.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiImageStripPreviewHost
{
public:
    virtual                     ~IDxuiImageStripPreviewHost() = default;

    virtual RECT                GetPreviewAnchorFor (SIZE sizeDip) const = 0;
    virtual DxuiPopupPlacement  GetPreviewPlacement () const             = 0;
};
