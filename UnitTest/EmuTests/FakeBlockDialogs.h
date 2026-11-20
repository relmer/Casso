#pragma once

#include "Pch.h"

#include "../CaptureTests/FakeHostDialogs.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeBlockDialogs
//
//  IBlockDialogs that lists the warnings and uses FakeHostDialogs for the
//  file dialogs.
//
////////////////////////////////////////////////////////////////////////////////

class FakeBlockDialogs : public IBlockDialogs
{
public:
    std::vector<std::wstring>  warnings;
    FakeHostDialogs            host;

    void                 ShowWarning    (const std::wstring & text) override { warnings.push_back (text); }
    HWND                 GetDialogOwner () const override                   { return nullptr; }
    const IDxuiTheme   * GetDialogTheme () const override                   { return nullptr; }
    IHostDialogs       & GetHostDialogs () override                         { return host; }
};
