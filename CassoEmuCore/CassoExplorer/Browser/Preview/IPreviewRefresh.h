#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IPreviewRefresh
//
//  The previewer as the preview's components call it, to make the preview
//  again after a choice changes.
//
////////////////////////////////////////////////////////////////////////////////

class IPreviewRefresh
{
public:
    virtual ~IPreviewRefresh() = default;

    virtual void  UpdatePreview () = 0;
};
