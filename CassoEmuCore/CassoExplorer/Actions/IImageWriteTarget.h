#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IImageWriteTarget
//
//  The image the raw sector and block verbs act on, and the work done after
//  one of them writes to it.
//
////////////////////////////////////////////////////////////////////////////////

class IImageWriteTarget
{
public:
    virtual ~IImageWriteTarget() = default;

    virtual std::wstring  GetFormatTarget () const                         = 0;
    virtual void          FinishWrite     (const std::wstring & imagePath) = 0;
};
