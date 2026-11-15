#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Picker/PickerScope.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickerScope::PickerScope
//
////////////////////////////////////////////////////////////////////////////////

PickerScope::PickerScope (const PickerRequest & request)
    : m_drive (request.receivingDrive)
{
    for (const FileDialogFilter & filter : request.filters)
    {
        m_masks.emplace_back (filter.pattern);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerScope::Passes
//
//  A stock disk's row was put in the list by the drive rule already.
//
////////////////////////////////////////////////////////////////////////////////

bool PickerScope::Passes (const CatalogRow & row) const
{
    if (row.isDirectory || row.isStockDisk)
    {
        return true;
    }

    if (!GetActiveMasks().Matches (row.name))
    {
        return false;
    }

    return !m_drive.has_value() || DriveImageRule::Accepts (*m_drive, row.imageClass);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerScope::SetActiveFilter
//
////////////////////////////////////////////////////////////////////////////////

void PickerScope::SetActiveFilter (size_t index)
{
    if (index < m_masks.size())
    {
        m_active = index;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerScope::GetActiveMasks
//
////////////////////////////////////////////////////////////////////////////////

const FileMaskSet & PickerScope::GetActiveMasks() const
{
    return (m_active < m_masks.size()) ? m_masks[m_active] : m_all;
}
