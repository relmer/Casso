#pragma once

#include "Pch.h"

#include "CassoLink/Session/CassoLinkCapture.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureStamp
//
//  Adds a format stamp to the WOZ a capture built: each track as the disk
//  inspector analyzes it, with the capture's weak, unstable and replaced
//  positions, its streams between whole tracks, its revolution time and its
//  track placement.
//
////////////////////////////////////////////////////////////////////////////////

class CaptureStamp
{
public:
    static HRESULT  AddToWoz (const std::vector<CassoLinkCapture::Position>  &  positions,
                              const CassoLinkCapture::Result                &  result,
                              const std::string                             &  written,
                              std::vector<Byte>                             &  ioWoz);
};
