#pragma once

#include "Debugger/IDiagnosticsProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDiagnosticsProvider
//
//  A device that is no device: one group, one byte with a two-bit decode, the
//  payload a test gives it, and a count of how often it was asked. A panel
//  that renders this renders anything a future device might publish.
//
////////////////////////////////////////////////////////////////////////////////

class FakeDiagnosticsProvider : public IDiagnosticsProvider
{
public:
    std::string        id      = "fake";
    std::string        title   = "Fake";
    Byte               value   = 0x80;
    DiagnosticsVisual  visual;
    mutable int        calls   = 0;

    std::string  GetDiagnosticsId    () const override { return id; }
    std::string  GetDiagnosticsTitle () const override { return title; }

    void GetDiagnostics (DiagnosticsSnapshot & snapshot) const override
    {
        calls++;
        snapshot.groups.push_back ({ "Group", { MakeByteRow ("Register", value, { "HI", "", "", "", "", "", "", "LO" }),
                                                MakeTextRow ("Name",     "text") } });
        snapshot.visual = visual;
    }
};
