#include "Pch.h"

#include "CassoExplorer/Model/FilesystemChoices.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FilesystemChoices::MakeKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FilesystemChoices::MakeKey (const std::wstring & imagePath)
{
    std::wstring  key = imagePath;



    std::transform (key.begin(), key.end(), key.begin(), towlower);

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesystemChoices::GetChoice
//
////////////////////////////////////////////////////////////////////////////////

VolumeKind FilesystemChoices::GetChoice (const std::wstring & imagePath) const
{
    auto  found = m_choices.find (MakeKey (imagePath));



    return (found == m_choices.end()) ? VolumeKind::Unknown : found->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesystemChoices::SetChoice
//
////////////////////////////////////////////////////////////////////////////////

void FilesystemChoices::SetChoice (const std::wstring & imagePath, VolumeKind kind)
{
    if (kind == VolumeKind::Unknown)
    {
        m_choices.erase (MakeKey (imagePath));
    }
    else
    {
        m_choices[MakeKey (imagePath)] = kind;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesystemChoices::GetWord
//
////////////////////////////////////////////////////////////////////////////////

const char * FilesystemChoices::GetWord (VolumeKind kind)
{
    return (kind == VolumeKind::Cpm) ? "cpm" : "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilesystemChoices::FromWord
//
////////////////////////////////////////////////////////////////////////////////

VolumeKind FilesystemChoices::FromWord (const std::string & word)
{
    return (_stricmp (word.c_str(), "cpm") == 0) ? VolumeKind::Cpm : VolumeKind::Unknown;
}
