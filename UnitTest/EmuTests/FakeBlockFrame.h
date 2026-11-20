#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeBlockFrame
//
//  IBlockFrame that counts closes and captions, and has the last caption.
//
////////////////////////////////////////////////////////////////////////////////

class FakeBlockFrame : public IBlockFrame
{
public:
    int           closes        = 0;
    int           closeRequests = 0;
    int           captions      = 0;
    std::wstring  caption;
    size_t        markAt        = std::wstring::npos;
    double        zoom          = 1.0;

    void    CloseWindow  () override                                         { closes++; }
    void    RequestClose () override                                         { closeRequests++; }
    void    MarkDirty    () override                                         {}
    void    SetCaption   (const std::wstring & title, size_t mark) override  { captions++; caption = title; markAt = mark; }
    double  GetZoom      () const override                                   { return zoom; }
    void    SetZoom      (double value) override                             { zoom = value; }
};
