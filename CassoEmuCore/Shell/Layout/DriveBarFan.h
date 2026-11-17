#pragma once

#include "Pch.h"

#include "Ui/Scene/DeskDrivePlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan
//
//  The drive bar for the classes the desk plan puts there (DeskDrivePlan):
//  one widget per class in a row along the bar, each class in a box, and the
//  class's other drives fanned out directly above its widget while the
//  pointer is on it, a widget each, like a menu opening from a menu bar.
//
//  The first drive of a class is the bottom one of its stack, so the boot
//  drive is the one on the bar. The fan slides up from behind the widget as
//  its progress runs from 0 to 1, its second drive nearest the widget, and
//  the class's box grows with it. The box takes in the widget, so the pointer
//  can travel from the widget into the fan without crossing a gap that would
//  close it.
//
//  With every class expanded (the auto-hide bar), each fan is open whatever
//  the pointer does.
//
////////////////////////////////////////////////////////////////////////////////

class DriveBarFan
{
public:
    //  The drives the plan puts on the drive bar, one list per class
    //  (floppies, then hard disks), each from the bottom of its stack up. A
    //  class with no drive on the bar has no list.
    static std::vector<std::vector<DeskDrive>>  Group (const std::vector<DeskDrivePlacement> & plan);

    //  Moves each of the `recent` drives to the bottom of its class, the
    //  slot drawn while the class is closed. A drive no longer on the bar
    //  changes nothing.
    static void  PutFirst (std::vector<std::vector<DeskDrive>> & classes, const std::vector<DeskDrive> & recent);

    //  The bar's zoom after `notches` of Ctrl+wheel, kZoomStep a notch,
    //  between kMinZoom and kMaxZoom.
    static constexpr float  kMinZoom  = 0.8f;
    static constexpr float  kMaxZoom  = 3.0f;
    static constexpr float  kZoomStep = 0.1f;

    static float  GetZoomAfterWheel (float zoom, float notches);

    void  SetClasses (std::vector<std::vector<DeskDrive>> classes);

    //  Width of a row of `classCount` widgets.
    static int  GetRowWidth (int classCount, int slotWidthPx, int gapPx);

    //  Lays the widgets out in a row from (originX, rowTop) and each fan
    //  above its widget. `panelPadPx` is the box's margin round the drives,
    //  and `arrowPx` the extra room at the right of a box whose fan is closed
    //  for the arrow that marks it.
    void  Layout (int originX, int rowTop, SIZE slotPx, int gapXPx, int gapYPx, int panelPadPx, int arrowPx);

    //  How far the open fans have slid up, from 0 (behind the widget) to 1.
    void   SetFanProgress (float progress) { m_fanProgress = std::clamp (progress, 0.0f, 1.0f); }
    float  GetFanProgress () const         { return m_fanProgress; }

    void   SetExpandAll   (bool expand)    { m_expandAll = expand; }
    bool   IsExpandAll    () const         { return m_expandAll; }

    //  Height of the tallest class, fully expanded, from the top of its box
    //  to the row's bottom.
    int    GetExpandedHeight () const;

    int                GetClassCount () const { return (int) m_classes.size(); }
    int                GetEntryCount (int classIndex) const;
    const DeskDrive &  GetDrive      (int classIndex, int entry) const;
    RECT               GetSlotRect   (int classIndex, int entry) const;
    bool               IsSlotShown   (int classIndex, int entry) const;
    bool               IsClassOpen   (int classIndex) const;
    int                GetRowTop     () const { return m_rowTop; }

    //  A class's box: its widget, and its fan while the fan is open.
    RECT               GetPanelRect  (int classIndex) const;
    int                GetOpenClass  () const { return m_openClass; }

    //  Opens the fan of the widget under the pointer. The fan remains open
    //  while the pointer is in the class's box. Returns true when the open
    //  fan changed. With every class expanded, nothing changes.
    bool  UpdateHover (POINT pt);
    void  Close       ();

    //  The visible drive under the pointer.
    bool  HitTest (POINT pt, int & outClass, int & outEntry) const;

private:
    std::vector<std::vector<DeskDrive>>  m_classes;
    int                                  m_originX     = 0;
    int                                  m_rowTop      = 0;
    SIZE                                 m_slot        = {};
    int                                  m_gapX        = 0;
    int                                  m_gapY        = 0;
    int                                  m_pad         = 0;
    int                                  m_arrow       = 0;
    int                                  m_openClass   = -1;
    float                                m_fanProgress = 1.0f;
    bool                                 m_expandAll   = false;
};
