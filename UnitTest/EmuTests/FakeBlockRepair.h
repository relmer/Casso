#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeBlockRepair
//
//  IBlockRepair that counts repairs, with CanRepair set by the test.
//
////////////////////////////////////////////////////////////////////////////////

class FakeBlockRepair : public IBlockRepair
{
public:
    bool  canRepair = true;
    int   repairs   = 0;

    bool  CanRepair () const override { return canRepair; }
    void  Repair    () override       { repairs++; }
};
