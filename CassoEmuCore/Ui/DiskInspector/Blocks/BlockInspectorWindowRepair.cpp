#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"
#include "Ui/DiskInspector/RepairDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::FormatCardIntro
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorWindow::FormatCardIntro (const std::wstring & fileName)
{
    std::wstring  name = RepairDialog::FormatName (fileName);



    return L"Repairing a partition writes a repaired copy of it to an image file you choose. " + name + L" on the card isn't changed. "
           L"Select the repairs below to apply to the repaired image.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::FormatCaution
//
//  For a volume that is only Possible inconsistencies, whose findings can
//  be normal for the disk: a DOS volume has the track inspector's caution,
//  as DOS Master's larger DOS image is in sectors that aren't in any file.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorWindow::FormatCaution (bool isDos)
{
    return isDos ? L"These findings can be normal on a disk made by another DOS or by a program that uses the VTOC or catalog differently. "
                   L"Repair a disk like this only when it doesn't work."
                 : L"These findings can be normal on a disk made by a program that allocates blocks differently. "
                   L"Repair a disk like this only when it doesn't work.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::IsOnRemovableDrive
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::IsOnRemovableDrive (const fs::path & path)
{
    wchar_t  root[MAX_PATH] = {};



    return GetVolumePathNameW (path.c_str(), root, MAX_PATH) && GetDriveTypeW (root) == DRIVE_REMOVABLE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::HasRoomFor
//
//  Room for the copy and a margin. A drive whose free space can't be read
//  is taken to have room, and the write reports a failure.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::HasRoomFor (const fs::path & folder, uint64_t bytes)
{
    ULARGE_INTEGER  available = {};



    return !GetDiskFreeSpaceExW (folder.c_str(), &available, nullptr, nullptr) || available.QuadPart >= bytes + kRoomMarginBytes;
}
