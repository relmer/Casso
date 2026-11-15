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
//  DriveBarFan::GetTitle
//
//  The //c's SmartPort is slot 5 to ProDOS, so a drive on its disk port
//  (slot 0 here) is addressed as slot 5. ProDOS addresses two drives a slot;
//  past those, a drive is given its unit number.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveBarFan::GetTitle (const DeskDrive & drive, bool numbered)
{
    constexpr int  kCSmartPortSlot = 5;
    std::wstring   title;
    int            slot            = (drive.slot == 0) ? kCSmartPortSlot : drive.slot;



    switch (drive.model)
    {
        case DeskDriveModel::DiskII:        title = std::format (L"Drive {} - Disk ][", drive.drive + 1);                            break;
        case DeskDriveModel::Disk2c:        title = (drive.drive == 0) ? std::wstring (L"Drive 1 - Built-in") : L"Drive 2 - Disk IIc";  break;
        case DeskDriveModel::UniDisk35:     title = L"UniDisk 3.5";                                                                  break;
        case DeskDriveModel::Apple35Drive:  title = L"Apple 3.5 Drive";                                                              break;
        case DeskDriveModel::Apple35Plus:   title = L"Built-in 3.5";                                                                 break;
        case DeskDriveModel::HardDisk20SC:  title = std::format (L"HD20SC - ID {}", drive.drive);                                    break;
        case DeskDriveModel::ProFile:       title = L"ProFile";                                                                      break;
    }

    if (numbered && drive.model != DeskDriveModel::DiskII && drive.model != DeskDriveModel::Disk2c && drive.model != DeskDriveModel::HardDisk20SC)
    {
        title += (drive.drive < 2) ? std::format (L" - S{},D{}", slot, drive.drive + 1) : std::format (L" - unit {}", drive.drive + 1);
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetDetail
//
//  A floppy drive's capacity, and the disk's size when a smaller disk is in
//  it. A hard disk's size is its image's, since the emulated disk is the
//  image's size, and a ProFile's is its fixed capacity.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveBarFan::GetDetail (DeskDriveModel model, uint32_t diskBlocks, uint32_t capacityBlocks)
{
    constexpr uint32_t  k35Blocks = 1600;
    std::wstring        detail;



    switch (model)
    {
        case DeskDriveModel::DiskII:
        case DeskDriveModel::Disk2c:
            detail = L"5.25\" (140K)";
            break;

        case DeskDriveModel::UniDisk35:
        case DeskDriveModel::Apple35Drive:
        case DeskDriveModel::Apple35Plus:
            detail = L"3.5\" (800K)";

            if (diskBlocks != 0 && diskBlocks < k35Blocks)
            {
                detail += std::format (L", {} disk", FormatSize (diskBlocks));
            }

            break;

        case DeskDriveModel::HardDisk20SC:
            detail = (diskBlocks != 0) ? std::format (L"Hard disk ({})", FormatSize (diskBlocks)) : std::wstring (L"Hard disk");
            break;

        case DeskDriveModel::ProFile:
            detail = std::format (L"Hard disk ({})", FormatSize (capacityBlocks));
            break;
    }

    return detail;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::FormatSize
//
//  Kilobytes below a megabyte, whole megabytes above.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveBarFan::FormatSize (uint32_t blocks)
{
    constexpr uint64_t  kBlockBytes = 512;
    constexpr uint64_t  kMegabyte   = 1024 * 1024;
    uint64_t            bytes       = (uint64_t) blocks * kBlockBytes;



    return (bytes < kMegabyte) ? std::format (L"{}K", bytes / 1024) : std::format (L"{} MB", (bytes + kMegabyte / 2) / kMegabyte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::Layout
//
//  Only the inputs are stored: the slots and boxes follow the open fan and
//  its progress, which change between layouts.
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFan::Layout (int originX, int rowTop, SIZE slotPx, int gapXPx, int gapYPx, int panelPadPx, int arrowPx)
{
    m_originX = originX;
    m_rowTop  = rowTop;
    m_slot    = slotPx;
    m_gapX    = gapXPx;
    m_gapY    = gapYPx;
    m_pad     = panelPadPx;
    m_arrow   = arrowPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetExpandedHeight
//
////////////////////////////////////////////////////////////////////////////////

int DriveBarFan::GetExpandedHeight() const
{
    int  tallest = 0;



    for (const std::vector<DeskDrive> & drives : m_classes)
    {
        tallest = std::max (tallest, (int) drives.size());
    }

    return (tallest == 0) ? 0 : tallest * (int) m_slot.cy + (tallest - 1) * m_gapY + m_pad;
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
//  DriveBarFan::IsClassOpen
//
////////////////////////////////////////////////////////////////////////////////

bool DriveBarFan::IsClassOpen (int classIndex) const
{
    return GetEntryCount (classIndex) > 1 && (m_expandAll || classIndex == m_openClass);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetSlotRect
//
//  The fan rises from behind the widget as one stack, keeping its spacing:
//  at progress 0 its top drive is level with the widget, and at 1 the stack
//  is in place. A fan drive below the widget's top is behind the widget.
//
////////////////////////////////////////////////////////////////////////////////

RECT DriveBarFan::GetSlotRect (int classIndex, int entry) const
{
    RECT   rc       = {};
    int    x        = m_originX + classIndex * ((int) m_slot.cx + m_gapX);
    int    step     = (int) m_slot.cy + m_gapY;
    float  progress = IsClassOpen (classIndex) ? m_fanProgress : 0.0f;
    int    rise     = 0;
    int    y        = 0;



    if (entry >= 0 && entry < GetEntryCount (classIndex))
    {
        rise = (entry == 0) ? 0 : (int) lroundf ((float) ((GetEntryCount (classIndex) - 1) * step) * (1.0f - progress));
        y    = std::min (m_rowTop, m_rowTop - entry * step + rise);
        rc   = { x, y, x + (int) m_slot.cx, y + (int) m_slot.cy };
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
    return entry >= 0 && entry < GetEntryCount (classIndex) && (entry == 0 || IsClassOpen (classIndex));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFan::GetPanelRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DriveBarFan::GetPanelRect (int classIndex) const
{
    RECT  rc    = {};
    RECT  head  = GetSlotRect (classIndex, 0);
    RECT  top   = head;
    int   count = GetEntryCount (classIndex);
    int   extra = (count > 1 && !m_expandAll) ? m_arrow : 0;



    if (count > 0)
    {
        if (IsClassOpen (classIndex))
        {
            top = GetSlotRect (classIndex, count - 1);
        }

        rc = { head.left - m_pad, top.top - m_pad, head.right + m_pad + extra, head.bottom + m_pad };
    }

    return rc;
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



    if (m_expandAll)
    {
        return false;
    }

    if (m_openClass >= 0 && m_openClass < (int) m_classes.size())
    {
        RECT  panel = GetPanelRect (m_openClass);

        if (PtInRect (&panel, pt))
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
//  The widget first: the fan drives slide out from behind it, so while they
//  overlap, the widget is the one in front.
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
