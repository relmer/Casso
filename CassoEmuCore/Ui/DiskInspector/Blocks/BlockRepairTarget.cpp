#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Seams/IDriveSpace.h"
#include "Ui/DiskInspector/Blocks/BlockRepairTarget.h"
#include "Ui/DiskInspector/RepairDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTarget::BuildIntro
//
//  The confirmation's introduction and its count of lines: the repaired
//  copy, and for a volume whose findings can be normal, a caution.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockRepairTarget::BuildIntro (bool isCard, const std::wstring & fileName, const std::wstring & hostFileName, bool isSuspect, bool isDos, int & outLines)
{
    std::wstring  intro = isCard ? FormatCardIntro (fileName) : RepairDialog::FormatIntro (hostFileName);



    outLines = kRepairIntroLines;

    if (isSuspect)
    {
        intro    += L"\n\n" + FormatCaution (isDos);
        outLines += kCautionLines;
    }

    return intro;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTarget::ChooseDestination
//
//  Beside the image, unless the image is on a removable drive, which can be
//  slow or short of space, or its drive doesn't have room for the copy. Then,
//  and for a card's partition, the user chooses where the copy goes.
//
////////////////////////////////////////////////////////////////////////////////

BlockRepairDestination BlockRepairTarget::ChooseDestination (const BlockRepairSource & source, IDiskFileIo & fileIo, const IDriveSpace & drives)
{
    BlockRepairDestination  destination;
    std::wstring            name   = RepairDialog::FormatName (source.hostFileName);
    std::filesystem::path   target;
    std::filesystem::path   host;



    if (source.isCard)
    {
        destination.bytes = source.volumeBytes;
        target            = std::filesystem::path ((source.volumeLabel.empty() ? std::wstring (L"Partition") : source.volumeLabel) + (source.isSectorVolume ? L" (repaired).dsk" : L" (repaired).po"));
        host              = target;
    }
    else
    {
        destination.bytes = drives.GetFileSize (TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (source.imagePath))));
        target            = TextEncoding::Utf8ToWide (RepairCopy::GetHostCopyPath (fileIo, TextEncoding::WideToUtf8 (source.imagePath)));
        host              = TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (target.wstring())));

        if (drives.IsOnRemovableDrive (host))
        {
            destination.why = name + L" is on a removable drive, which can be slow or short of space. Choose where to save the repaired copy.";
        }
        else if (!drives.HasRoomFor (host.parent_path(), destination.bytes))
        {
            destination.why = L"The drive with " + name + L" doesn't have room for the repaired copy. Choose where to save it.";
        }
    }

    //  For an image in a ZIP, the user chooses where the copy of the ZIP goes.
    destination.host         = host;
    destination.rest         = target.wstring().substr (host.wstring().size());
    destination.isUserChoice = source.isCard || !destination.why.empty();

    return destination;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTarget::FormatCardIntro
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockRepairTarget::FormatCardIntro (const std::wstring & fileName)
{
    std::wstring  name = RepairDialog::FormatName (fileName);



    return L"Repairing a partition writes a repaired copy of it to an image file you choose. " + name + L" on the card isn't changed. "
           L"Select the repairs below to apply to the repaired image.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTarget::FormatCaution
//
//  For a volume that is only Possible inconsistencies, whose findings can
//  be normal for the disk: a DOS volume has the track inspector's caution,
//  as DOS Master's larger DOS image is in sectors that aren't in any file.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockRepairTarget::FormatCaution (bool isDos)
{
    return isDos ? L"These findings can be normal on a disk made by another DOS or by a program that uses the VTOC or catalog differently. "
                   L"Repair a disk like this only when it doesn't work."
                 : L"These findings can be normal on a disk made by a program that allocates blocks differently. "
                   L"Repair a disk like this only when it doesn't work.";
}
