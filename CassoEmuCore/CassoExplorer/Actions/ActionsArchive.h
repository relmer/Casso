#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"

class CassoExplorerActions;
enum class DiskImageFormat;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsArchive
//
//  Archives made from what is selected: a ShrinkIt archive of files and
//  folders on the PC, put in as a put into a disk image puts them, and a
//  ShrinkIt disk of a disk image, and a disk image in another image format.
//  Each is written beside the first item, under a name nothing in the
//  folder has. Test reports what `casso disk test` finds in an image or an
//  archive.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsArchive
{
public:
    explicit ActionsArchive (CassoExplorerActions & actions) : m_actions (actions) {}

    ActionsTypes::Outcome  MakeArchive      (const std::vector<std::wstring> & hostPaths, std::wstring & outPath);
    ActionsTypes::Outcome  MakeShrinkItDisk (const std::wstring & imagePath, std::wstring & outPath);
    ActionsTypes::Outcome  ConvertImage     (const std::wstring & imagePath, DiskImageFormat format, std::wstring & outPath);
    ActionsTypes::Outcome  Test             (const std::wstring & imagePath, std::wstring & outReport);

    //  Whether a disk image can be made into a ShrinkIt disk: not an archive,
    //  nor a ShrinkIt disk already.
    static bool  CanMakeShrinkItDisk (const std::wstring & imagePath);

    //  The formats an image converts to, which depend on its size.
    void  GetConvertTargets (const std::wstring & imagePath, std::vector<DiskImageFormat> & outTargets) const;

    //  The Convert to verb for each format, and the format for each such verb.
    static ActionsTypes::Verb  GetConvertVerb      (DiskImageFormat format);
    static bool                TryGetConvertFormat (ActionsTypes::Verb verb, DiskImageFormat & outFormat);

private:
    CassoExplorerActions & m_actions;
};
