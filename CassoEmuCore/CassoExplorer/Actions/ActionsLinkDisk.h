#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"

class CassoExplorerBrowser;
struct CassoLinkSettings;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsLinkDisk
//
//  The CassoLink boot media: the boot disk, the ProDOS and SOS virtual drives
//  disks and the cassette bootstrap, saved as files on the PC or written to a
//  linked Apple's drive.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsLinkDisk
{
public:
    explicit ActionsLinkDisk (CassoExplorerBrowser & browser);

    ActionsTypes::Outcome  SaveLinkToDisk        (const std::string & link, int drive);
    ActionsTypes::Outcome  SaveLinkDiskImage     (const std::wstring & imagePath, const CassoLinkSettings & settings);
    ActionsTypes::Outcome  SaveLinkProDosDisk    (const std::wstring & usersDisk, const std::wstring & imagePath, const CassoLinkSettings & settings);
    ActionsTypes::Outcome  SaveLinkProDosToDisk  (const std::string & link, int drive, const std::wstring & usersDisk);
    ActionsTypes::Outcome  SaveLinkSosDisk       (const std::wstring & usersDisk, const std::wstring & imagePath, const CassoLinkSettings & settings);
    ActionsTypes::Outcome  SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand);

private:
    CassoLinkSettings      FindLinkSettings   (const std::string & link) const;
    ActionsTypes::Outcome  MakeLinkProDosDisk (const std::wstring & usersDisk, const CassoLinkSettings & settings, std::vector<Byte> & outDisk);

    CassoExplorerBrowser & m_browser;
};
