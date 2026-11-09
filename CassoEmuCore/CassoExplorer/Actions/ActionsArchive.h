#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"

class CassoExplorerActions;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsArchive
//
//  Archives made from what is selected: a ShrinkIt archive of files and
//  folders on the PC, put in as a put into a disk image puts them, and a
//  ShrinkIt disk of a disk image. Each is written beside the first item,
//  under a name nothing in the folder has. Test reports what `casso disk
//  test` finds in an image or an archive.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsArchive
{
public:
    explicit ActionsArchive (CassoExplorerActions & actions) : m_actions (actions) {}

    ActionsTypes::Outcome  MakeArchive      (const std::vector<std::wstring> & hostPaths, std::wstring & outPath);
    ActionsTypes::Outcome  MakeShrinkItDisk (const std::wstring & imagePath, std::wstring & outPath);
    ActionsTypes::Outcome  Test             (const std::wstring & imagePath, std::wstring & outReport);

    //  Whether a disk image can be made into a ShrinkIt disk: not an archive,
    //  nor a ShrinkIt disk already.
    static bool  CanMakeShrinkItDisk (const std::wstring & imagePath);

private:
    CassoExplorerActions & m_actions;
};
