#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorCommandInterfaces.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"

class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet
//
//  The disk inspector's commands for the menus, the toolbar and the keys
//  (FR-158), and the window's shortcut keys that run them.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorCommandSet
{
public:
    InspectorCommandSet (InspectorSession & session, IInspectorComponentHost & host, IInspectorFrame & frame, IInspectorParts & parts, InspectorCommands & commands);

    //  Every command's label, shortcut, icon, action and state. Casso's head
    //  and drive commands are left empty in a host without drives.
    void  Build ();

    //  A command's action, if it has one and is available now; true if it
    //  ran.
    static bool  Run (const InspectorCommands::Command & command);

    //  The window's shortcut keys, after the menus, the find bar, the
    //  focused control and editing have had the key. True when the key is
    //  taken.
    bool  RunKey (const DxuiKeyEvent & ev);

private:
    static bool  IsZoomKey     (WPARAM vk);
    int          StepRecording (int quarterTrack, int step) const;
    void         TransferRaw   (bool isSave);

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    IInspectorFrame          & m_frame;
    IInspectorParts          & m_parts;
    InspectorCommands        & m_commands;
};
