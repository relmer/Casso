#include "Pch.h"

#include "Shell/Layout/DriveBarFan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::Group
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DeskDrive>> DriveBarFan::Group (const std::vector<DeskDrivePlacement> & plan)
{
    std::vector<std::vector<DeskDrive>>  out;



    for (DeskDriveClass driveClass : { DeskDriveClass::Floppy, DeskDriveClass::HardDisk })
    {
        std::vector<DeskDrivePlacement>  members;
        std::vector<DeskDrive>           drives;



        for (const DeskDrivePlacement & placement : plan)
        {
            if (placement.spot == DeskDriveSpot::DriveBar && DeskDrivePlan::GetClass (placement.drive.model) == driveClass)
            {
                members.push_back (placement);
            }
        }

        std::stable_sort (members.begin(), members.end(),
                          [] (const DeskDrivePlacement & a, const DeskDrivePlacement & b) { return a.level < b.level; });

        for (const DeskDrivePlacement & placement : members)
        {
            drives.push_back (placement.drive);
        }

        if (!drives.empty())
        {
            out.push_back (std::move (drives));
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::SetClasses
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFan::SetClasses (std::vector<std::vector<DeskDrive>> classes)
{
    m_classes = std::move (classes);
    m_slots.assign (m_classes.size(), {});
    m_panels.assign (m_classes.size(), RECT {});

    if (m_openClass >= (int) m_classes.size())
    {
        m_openClass = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetRowWidth
//
////////////////////////////////////////////////////////////////////////////////

int DriveBarFan::GetRowWidth (int classCount, int slotWidthPx, int gapPx)
{
    return (classCount <= 0) ? 0 : classCount * slotWidthPx + (classCount - 1) * gapPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFan::Layout (int originX, int rowTop, SIZE slotPx, int gapXPx, int gapYPx, int panelPadPx)
{
    for (size_t c = 0; c < m_classes.size(); c++)
    {
        int  x = originX + (int) c * (slotPx.cx + gapXPx);



        m_slots[c].assign (m_classes[c].size(), RECT {});
        m_panels[c] = {};

        for (size_t i = 0; i < m_classes[c].size(); i++)
        {
            int  y = rowTop - (int) i * (slotPx.cy + gapYPx);

            m_slots[c][i] = { x, y, x + slotPx.cx, y + slotPx.cy };
        }

        if (m_classes[c].size() > 1)
        {
            const RECT &  top = m_slots[c].back();

            m_panels[c] = { x - panelPadPx, top.top - panelPadPx, x + slotPx.cx + panelPadPx, rowTop };
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetEntryCount
//
////////////////////////////////////////////////////////////////////////////////

int DriveBarFan::GetEntryCount (int classIndex) const
{
    return (classIndex < 0 || classIndex >= (int) m_classes.size()) ? 0 : (int) m_classes[(size_t) classIndex].size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetDrive
//
////////////////////////////////////////////////////////////////////////////////

const DeskDrive & DriveBarFan::GetDrive (int classIndex, int entry) const
{
    _ASSERTE (entry >= 0 && entry < GetEntryCount (classIndex));

    return m_classes[(size_t) classIndex][(size_t) entry];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetSlotRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DriveBarFan::GetSlotRect (int classIndex, int entry) const
{
    RECT  rc = {};



    if (entry >= 0 && entry < GetEntryCount (classIndex) && (size_t) entry < m_slots[(size_t) classIndex].size())
    {
        rc = m_slots[(size_t) classIndex][(size_t) entry];
    }

    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::IsSlotShown
//
//  A class's widget is always visible; the rest only while its fan is open.
//
////////////////////////////////////////////////////////////////////////////////

bool DriveBarFan::IsSlotShown (int classIndex, int entry) const
{
    return entry >= 0 && entry < GetEntryCount (classIndex) && (entry == 0 || classIndex == m_openClass);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetPanelRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DriveBarFan::GetPanelRect() const
{
    return (m_openClass < 0 || m_openClass >= (int) m_panels.size()) ? RECT {} : m_panels[(size_t) m_openClass];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::UpdateHover
//
////////////////////////////////////////////////////////////////////////////////

bool DriveBarFan::UpdateHover (POINT pt)
{
    int  was  = m_openClass;
    int  open = -1;



    if (m_openClass >= 0 && m_openClass < (int) m_classes.size())
    {
        RECT  head  = GetSlotRect (m_openClass, 0);
        RECT  panel = m_panels[(size_t) m_openClass];

        if (PtInRect (&head, pt) || PtInRect (&panel, pt))
        {
            open = m_openClass;
        }
    }

    for (int c = 0; c < (int) m_classes.size() && open < 0; c++)
    {
        RECT  head = GetSlotRect (c, 0);

        if (m_classes[(size_t) c].size() > 1 && PtInRect (&head, pt))
        {
            open = c;
        }
    }

    m_openClass = open;

    return m_openClass != was;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::Close
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFan::Close()
{
    m_openClass = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DriveBarFan::HitTest (POINT pt, int & outClass, int & outEntry) const
{
    bool  hit = false;



    for (int c = 0; c < (int) m_classes.size() && !hit; c++)
    {
        for (int i = 0; i < GetEntryCount (c) && !hit; i++)
        {
            RECT  rc = GetSlotRect (c, i);

            if (IsSlotShown (c, i) && PtInRect (&rc, pt))
            {
                outClass = c;
                outEntry = i;
                hit      = true;
            }
        }
    }

    return hit;
}
