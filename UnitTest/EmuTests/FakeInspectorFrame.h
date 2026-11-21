#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorCommandInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeInspectorFrame
//
//  A fake disk inspector window for its commands, with a count of each call.
//
////////////////////////////////////////////////////////////////////////////////

class FakeInspectorFrame : public IInspectorFrame
{
public:
    double  zoom         = 1.0;
    int     closes       = 0;
    int     defaultSizes = 0;
    int     tipHides     = 0;

    void    CloseWindow        () override       { closes++; }
    double  GetWindowZoom      () const override { return zoom; }
    void    RestoreDefaultSize () override       { defaultSizes++; }
    void    HideTooltip        () override       { tipHides++; }
};
