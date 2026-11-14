#pragma once

#include "Pch.h"

#include "Ui/Scene/DeskDrivePlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan
//
//  The drive bar for the classes the desk plan puts there (DeskDrivePlan):
//  one widget per class in a row along the bar, and the class's other drives
//  fanned out directly above its widget while the pointer is on it, a widget
//  each, like a menu opening from a menu bar.
//
//  The first drive of a class is the bottom one of its stack, so the boot
//  drive is the one on the bar. The fan grows upward from the bar, its
//  second drive nearest the widget. The panel behind the fan extends down to
//  the widget's top, so the pointer can travel from the widget into the fan
//  without crossing a gap that would close it.
//
////////////////////////////////////////////////////////////////////////////////

class DriveBarFan
{
public:
    //  The drives the plan puts on the drive bar, one list per class
    //  (floppies, then hard disks), each from the bottom of its stack up. A
    //  class with no drive on the bar has no list.
    static std::vector<std::vector<DeskDrive>>  Group (const std::vector<DeskDrivePlacement> & plan);

    void  SetClasses (std::vector<std::vector<DeskDrive>> classes);

    //  Width of a row of `classCount` widgets.
    static int  GetRowWidth (int classCount, int slotWidthPx, int gapPx);

    //  Lays the widgets out in a row from (originX, rowTop) and each fan
    //  above its widget. `panelPadPx` is the panel's margin round the fan.
    void  Layout (int originX, int rowTop, SIZE slotPx, int gapXPx, int gapYPx, int panelPadPx);

    int                GetClassCount () const { return (int) m_classes.size(); }
    int                GetEntryCount (int classIndex) const;
    const DeskDrive &  GetDrive      (int classIndex, int entry) const;
    RECT               GetSlotRect   (int classIndex, int entry) const;
    bool               IsSlotShown   (int classIndex, int entry) const;

    //  The open fan's panel, or an empty rect when no fan is open.
    RECT               GetPanelRect  () const;
    int                GetOpenClass  () const { return m_openClass; }

    //  Opens the fan of the widget under the pointer. The fan remains open
    //  while the pointer is on the widget or the panel. Returns true when the
    //  open fan changed.
    bool  UpdateHover (POINT pt);
    void  Close       ();

    //  The visible drive under the pointer.
    bool  HitTest (POINT pt, int & outClass, int & outEntry) const;

private:
    std::vector<std::vector<DeskDrive>>  m_classes;
    std::vector<std::vector<RECT>>       m_slots;
    std::vector<RECT>                    m_panels;
    int                                  m_openClass = -1;
};
