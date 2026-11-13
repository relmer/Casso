#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "Devices/Disk/A2retroNetConfig.h"

class CassoExplorerActions;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet
//
//  A disk image on an A2Pico's micro SD card or USB drive set as the image
//  of the A2retroNET firmware's first drive with none, in the A2retroNET.txt
//  in the card's root. The image is checked first, as the firmware reads
//  it, and an image already set or a card with each drive set is reported.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsA2retroNet
{
public:
    //  A drive in use, from number= in A2retroNET.txt, and its image as the
    //  firmware would open it.
    struct Drive
    {
        int           number           = 0;
        std::string   path;
        bool          isPresent        = false;
        uint64_t      bytes            = 0;
        bool          isWriteProtected = false;
        std::wstring  problem;
    };

    struct Card
    {
        std::wstring        root;
        A2retroNetConfig    config;
        std::vector<Drive>  drives;
    };

    explicit ActionsA2retroNet (CassoExplorerActions & actions) : m_actions (actions) {}

    //  Whether the folder is a drive's root with an A2retroNET.txt, which
    //  makes it an A2Pico card.
    bool     IsCard       (const std::wstring & folder) const;

    HRESULT  ReadCard     (const std::wstring & root, Card & outCard);

    //  A2retroNET.txt written again with LF line ends.
    ActionsTypes::Outcome  Repair (const std::wstring & root);

    //  The card's drives in a sentence, and each drive's problem.
    static std::wstring  Summarize (const Card & card);

    //  Whether the image's drive root has an A2retroNET.txt.
    bool  CanAdd (const std::wstring & imagePath) const;

    ActionsTypes::Outcome  Add (const std::wstring & imagePath);

    //  An A2retroNET.txt of the firmware's defaults in the root, for a card
    //  just initialized for an A2Pico. A root that has one is left as it is.
    ActionsTypes::Outcome  CreateConfig (const std::wstring & root);

private:
    static std::wstring  GetConfigPath (const std::wstring & imagePath);

    CassoExplorerActions & m_actions;
};
