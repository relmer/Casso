#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"

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
    explicit ActionsA2retroNet (CassoExplorerActions & actions) : m_actions (actions) {}

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
