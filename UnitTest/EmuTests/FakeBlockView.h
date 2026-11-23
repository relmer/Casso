#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeBlockView
//
//  IBlockView that counts refreshes, and has the editing state and the
//  edit controls' last state.
//
////////////////////////////////////////////////////////////////////////////////

class FakeBlockView : public IBlockView
{
public:
    int              refreshes = 0;
    bool             isEditing = false;
    BlockEditState   lastState;
    std::wstring     blockText;
    BlockHeaderView  header;

    void               Refresh        () override                             { refreshes++; }
    void               SetEditing     (bool editing) override                 { isEditing = editing; }
    bool               IsEditing      () const override                       { return isEditing; }
    void               SetEditState   (const BlockEditState & state) override { lastState = state; }
    BlockHeaderView  & GetHeader      () override                             { return header; }
    BlockHexView     * GetHexView     () const override                       { return nullptr; }
    std::wstring       BuildBlockText () const override                       { return blockText; }
};
