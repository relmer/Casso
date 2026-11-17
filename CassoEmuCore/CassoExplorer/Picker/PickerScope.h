#pragma once

#include "Pch.h"

#include "CassoExplorer/Picker/FileMaskSet.h"
#include "Devices/Disk/DriveImageRule.h"

struct CatalogRow;
struct PickerRequest;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerScope
//
//  Which rows a picker lists: every folder, and the files whose names match
//  the active filter entry's masks and, for a drive's picker, whose images
//  the drive accepts.
//
////////////////////////////////////////////////////////////////////////////////

class PickerScope
{
public:
    PickerScope () = default;
    explicit PickerScope (const PickerRequest & request);

    bool    Passes          (const CatalogRow & row) const;

    size_t  GetFilterCount  () const { return m_masks.size(); }
    size_t  GetActiveFilter () const { return m_active; }
    void    SetActiveFilter (size_t index);

    const FileMaskSet &  GetActiveMasks () const;

private:
    std::vector<FileMaskSet>       m_masks;
    size_t                         m_active = 0;
    std::optional<ReceivingDrive>  m_drive;
    FileMaskSet                    m_all;
};
