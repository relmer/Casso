#include "Pch.h"

#include "Ui/DiskInspector/InspectorFilesystemOffer.h"
#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFilesystemOffer::GetOffer
//
//  A 13-sector disk isn't offered CP/M: its map isn't "no file system".
//
////////////////////////////////////////////////////////////////////////////////

InspectorFilesystemOffer::Offer InspectorFilesystemOffer::GetOffer (const DiskAnalysis & analysis, const IDiskInspectorHost & host)
{
    bool  isNone = analysis.fileMaps.size() == 1 && analysis.fileMaps[0].notMapped == NotMappedReason::NoFileSystem;



    if (!host.CanChooseFilesystem())
    {
        return Offer::None;
    }

    if (analysis.isCpmChosen)
    {
        return Offer::DetectFilesystem;
    }

    return isNone ? Offer::ReadAsCpm : Offer::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFilesystemOffer::GetNote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFilesystemOffer::GetNote (Offer offer)
{
    std::wstring  note;



    if (offer == Offer::ReadAsCpm)
    {
        note = L"No standard file system was found. If this is a CP/M disk with a damaged directory, try Read as CP/M.";
    }
    else if (offer == Offer::DetectFilesystem)
    {
        note = L"This disk is read as CP/M.";
    }

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFilesystemOffer::GetButtonLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFilesystemOffer::GetButtonLabel (Offer offer)
{
    std::wstring  label;



    if (offer == Offer::ReadAsCpm)
    {
        label = L"Read as CP/M";
    }
    else if (offer == Offer::DetectFilesystem)
    {
        label = L"Detect file system";
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFilesystemOffer::Accept
//
////////////////////////////////////////////////////////////////////////////////

void InspectorFilesystemOffer::Accept (Offer offer, IDiskInspectorHost & host, DiskAnalysis & inOut)
{
    if (offer == Offer::None)
    {
        return;
    }

    inOut.isCpmChosen = offer == Offer::ReadAsCpm;
    host.SetReadAsCpm (inOut.isCpmChosen);
}





