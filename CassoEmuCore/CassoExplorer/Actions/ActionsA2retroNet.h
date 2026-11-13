#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "Devices/Disk/A2retroNetConfig.h"
#include "Devices/Disk/IDiskFileIo.h"

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

    //  A drive, from 1, with the image at `path` as the firmware would open
    //  it. A drive with no path is empty and has no problem.
    Drive    CheckDrive   (const std::wstring & root, int number, const std::string & path);

    //  The size and time of the card's A2retroNET.txt, which change when
    //  another program saves it.
    HRESULT  GetConfigStamp (const std::wstring & root, FileStamp & outStamp);

    //  A2retroNET.txt written again with LF line ends.
    ActionsTypes::Outcome  Repair (const std::wstring & root);

    //  The settings written to the card's A2retroNET.txt.
    ActionsTypes::Outcome  Save   (const std::wstring & root, const A2retroNetConfig & config);

    //  Each .po, .hdv and .2mg on the card, in its folders too, as a path
    //  from the root with / between folders, in order. At most kImagesMax.
    HRESULT  ListImages (const std::wstring & root, std::vector<std::string> & outPaths);

    //  The card's drives in a sentence, and each drive's problem.
    static std::wstring  Summarize (const Card & card);

    //  Whether the image's drive root has an A2retroNET.txt.
    bool  CanAdd (const std::wstring & imagePath) const;

    ActionsTypes::Outcome  Add (const std::wstring & imagePath);

    //  An A2retroNET.txt of the firmware's defaults in the root, for a card
    //  just initialized for an A2Pico. A root that has one is left as it is.
    ActionsTypes::Outcome  CreateConfig (const std::wstring & root);

private:
    //  A card's whole tree isn't listed for a dropdown, which a card full of
    //  images makes too long to use anyway.
    static constexpr size_t  kImagesMax = 2000;

    static std::wstring  GetConfigPath (const std::wstring & imagePath);

    //  The images in the folder and its subfolders, with `prefix` before
    //  each path.
    HRESULT  ListImagesIn (const std::wstring & folder, const std::string & prefix, std::vector<std::string> & ioPaths);

    CassoExplorerActions & m_actions;
};
