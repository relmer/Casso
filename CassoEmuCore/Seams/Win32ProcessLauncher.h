#pragma once

#include "Pch.h"
#include "Seams/IProcessLauncher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ProcessLauncher
//
//  CreateProcessW over the launcher seam. The child inherits no handles and
//  runs in its executable's own directory, so it finds its resources the way a
//  double-click would.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ProcessLauncher : public IProcessLauncher
{
public:
    HRESULT  Launch (const std::wstring & exePath, const std::wstring & arguments) override;
    bool     Exists (const std::wstring & exePath) override;
};
