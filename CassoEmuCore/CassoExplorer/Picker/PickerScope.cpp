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
//  A stock disk's row was put in the list by the drive rule already, so only
//  the filter applies to it, by the name of its file.
//
////////////////////////////////////////////////////////////////////////////////

bool PickerScope::Passes (const CatalogRow & row) const
{
    if (row.isDirectory)
    {
        return true;
    }

    if (row.isStockDisk)
    {
        return GetActiveMasks().Matches (std::filesystem::path (row.hostPath).filename().wstring());
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
