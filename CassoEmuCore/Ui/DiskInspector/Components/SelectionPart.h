#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"

class FindPart;
class IInspectorComponentHost;
class InspectorDialogs;
class StripControls;





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart
//
//  Which view the keys act on and how they move between views (FR-029,
//  FR-059), the selection's keys (FR-043), Copy and Copy sector (FR-057),
//  and the views' context menu.
//
////////////////////////////////////////////////////////////////////////////////

class SelectionPart : public IInspectorComponent
{
public:
    SelectionPart (InspectorSession & session, IInspectorComponentHost & host, InspectorDialogs & dialogs, FindPart & find, StripControls & stripControls);

    void  Create () override {}

    KeyTarget  GetKeyTarget       (POINT pointPx) const;
    void       StepKeyTarget      (int step);
    RECT       GetKeyTargetBounds () const;
    void       StepSector         (int delta, bool isToEnd);
    void       StepHistory        (bool isForward);
    void       RecordSelection    (bool isHeadSelecting);
    bool       OnHistoryButton    (const DxuiMouseEvent & ev);
    void       SelectAll          ();
    void       ExtendSelection    (WPARAM vk);
    void       Copy               ();
    void       CopySector         ();
    void       ShowContextMenu    (POINT pointPx);

    static bool  IsStripKey (WPARAM vk);

private:
    static bool  IsInside (const RECT & rect, POINT pointPx);

    InspectorSession                      & m_session;
    IInspectorComponentHost               & m_host;
    InspectorDialogs                      & m_dialogs;
    FindPart                              & m_findPart;
    StripControls                         & m_stripControls;
    vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;
};