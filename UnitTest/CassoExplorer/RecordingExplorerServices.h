#pragma once

#include "Pch.h"

#include "FakeRecycleBin.h"
#include "Seams/IInfoTips.h"
#include "Seams/IShellIcons.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Recording services
//
//  A Casso Explorer window's services, as fakes: each stores the arguments
//  of the calls a test checks and returns nothing.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingShellVerbs : public FakeRecycleBin
{
public:
    void  WatchRecycleBin (HWND, UINT message) override { recycleBinMessage = message; }

    UINT  recycleBinMessage = 0;
};



class RecordingInfoTips : public IInfoTips
{
public:
    void  Start  (HWND, UINT message) override                    { startMessage = message; }
    bool  TryGet (const std::wstring &, std::wstring &) override  { return false; }
    void  Clear  () override                                      {}

    UINT  startMessage = 0;
};



class BlankShellIcons : public IShellIcons
{
public:
    std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring &, bool) override { return nullptr; }
    std::shared_ptr<const DxuiIconImage>  GetForKind (Kind) override                       { return nullptr; }
    void                                  SetSizePx  (int px) override                     { sizePx = px; }

    int  sizePx = 0;
};
