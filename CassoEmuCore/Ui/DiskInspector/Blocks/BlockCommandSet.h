#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockBrowser.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"

class BlockInspectorModel;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet
//
//  Block mode's commands in the slots of the track inspector's menus, and
//  the keys and mouse buttons that run them.
//
////////////////////////////////////////////////////////////////////////////////

class BlockCommandSet
{
public:
    //  The parts of block mode that the commands call.
    struct Parts
    {
        BlockInspectorModel  & model;
        IBlockJobs           & jobs;
        IBlockView           & view;
        IBlockNavigation     & navigation;
        IBlockDisplay        & display;
        IBlockDialogs        & dialogs;
        IBlockFrame          & frame;
        IBlockFileInfo       & file;
        IBlockFileSession    & session;
        IBlockEditing        & editing;
        IBlockRepair         & repair;
    };

    BlockCommandSet (const Parts & parts, InspectorCommands & commands);

    void         Build             ();
    static bool  Run               (const InspectorCommands::Command & command);
    bool         TryRunWindowKey   (const DxuiKeyEvent & ev, bool & outResult);
    bool         TryRunKey         (const DxuiKeyEvent & ev, bool & outResult);
    bool         TryRunMouseButton (const DxuiMouseEvent & ev);

private:
    static constexpr const wchar_t *  kpszGlyphGoTo = L"\xE8AD";
    static constexpr const wchar_t *  kpszGlyphCopy = L"\xE8C8";
    static constexpr const wchar_t *  kpszGlyphEdit = L"\xE70F";

    void  ShowLevel      (BlockPlace::Kind kind);
    void  GoToBlock      ();
    void  StepDifference (bool isForward);

    Parts                m_parts;
    InspectorCommands  & m_commands;
};
