#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorStatusBar.h"

class BlockInspectorModel;
struct BlockEditState;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorState
//
//  What block mode's window draws from the model: its title, the edit
//  controls' state and the status band's left end.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorState
{
public:
    static std::wstring                          BuildTitle        (const std::wstring & fileName, const std::wstring & note, bool hasEdits, size_t & outMarkAt);
    static BlockEditState                        BuildEditState    (const BlockInspectorModel & model, bool isBusy, bool isEditing, const std::wstring & note);
    static vector<InspectorStatusBar::LeadItem>  BuildLead         (const BlockInspectorModel & model, bool isBusy, const std::wstring & selectionLine);

private:
    static constexpr const wchar_t *  kpszTitlePrefix = L"Block inspector - ";
};
