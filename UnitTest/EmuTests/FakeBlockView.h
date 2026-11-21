#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"
#include "Ui/DiskInspector/EditBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeBlockView
//
//  IBlockView that counts refreshes, and has the editing state and the
//  edit bar's last state.
//
////////////////////////////////////////////////////////////////////////////////

class FakeBlockView : public IBlockView
{
public:
    int           refreshes = 0;
    bool          isEditing = false;
    EditBarState  lastState;
    std::wstring  blockText;

    void            Refresh        () override                           { refreshes++; }
    void            SetEditing     (bool editing) override               { isEditing = editing; }
    bool            IsEditing      () const override                     { return isEditing; }
    void            SetEditState   (const EditBarState & state) override { lastState = state; }
    EditBar       & GetEditBar     () const override                     { return m_bar; }
    BlockHexView  * GetHexView     () const override                     { return nullptr; }
    std::wstring    BuildBlockText () const override                     { return blockText; }

private:
    mutable EditBar  m_bar;
};
