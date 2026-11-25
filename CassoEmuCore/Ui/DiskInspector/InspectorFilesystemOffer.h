#pragma once

#include "Pch.h"

class IDiskInspectorHost;
struct DiskAnalysis;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFilesystemOffer
//
//  Read as CP/M on a 5.25" disk where no standard file system is found, and
//  Detect file system once it's read that way. The choice is the host's for
//  the image, so Casso Explorer reads the image the same way.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorFilesystemOffer
{
public:
    enum class Offer
    {
        None,
        ReadAsCpm,
        DetectFilesystem,
    };

    static Offer         GetOffer       (const DiskAnalysis & analysis, const IDiskInspectorHost & host);
    static std::wstring  GetNote        (Offer offer);
    static std::wstring  GetButtonLabel (Offer offer);

    //  The host's choice and the analysis's flag for the offer. The caller
    //  assembles the analysis again.
    static void          Accept         (Offer offer, IDiskInspectorHost & host, DiskAnalysis & inOut);
};
