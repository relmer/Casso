#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames
//
//  The user's name for each Full Send image, by the fingerprint of the
//  image's disk: given.json in the thumbnail cache folder. A later capture
//  of a disk with that fingerprint is saved with its number and the name.
//
//  A name is stored without the image's number and extension, so 0024
//  Archon.woz is stored as Archon.
//
////////////////////////////////////////////////////////////////////////////////

class GivenNames
{
public:
    static constexpr const wchar_t *  kFileName = L"given.json";

    explicit GivenNames (std::filesystem::path folder);

    //  The name in a file name: the part after a leading number and its
    //  space, without the extension. Empty for a number alone.
    static std::string  GetGivenPart (const std::string & fileName);

    //  The name for the fingerprint, replacing any before it, and written to
    //  the file. An empty name removes the fingerprint's.
    HRESULT  Set    (const std::string & fingerprint, const std::string & name);

    bool     TryGet (const std::string & fingerprint, std::string & outName);

private:
    HRESULT  Load ();
    HRESULT  Save () const;

    std::filesystem::path               m_path;
    std::mutex                          m_lock;
    std::map<std::string, std::string>  m_names;
};
