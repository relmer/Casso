#pragma once

#include "Pch.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  FilesystemChoices
//
//  The file system the user chose for each image detection doesn't recognize,
//  by the image's path without regard to case. A choice applies only to an image
//  with no recognized file system.
//
////////////////////////////////////////////////////////////////////////////////

class FilesystemChoices
{
public:
    //  Unknown when the image has no choice.
    VolumeKind  GetChoice (const std::wstring & imagePath) const;

    //  Unknown removes the choice.
    void        SetChoice (const std::wstring & imagePath, VolumeKind kind);

    //  The word a choice has in the preferences and on the command line, and
    //  the choice for a word. Only CP/M can be chosen: it's the one file system
    //  with no signature.
    static const char *  GetWord  (VolumeKind kind);
    static VolumeKind    FromWord (const std::string & word);

    const std::map<std::wstring, VolumeKind> &  GetAll () const { return m_choices; }

private:
    static std::wstring  MakeKey (const std::wstring & imagePath);

    std::map<std::wstring, VolumeKind>  m_choices;
};
