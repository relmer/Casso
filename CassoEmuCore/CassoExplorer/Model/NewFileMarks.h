#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NewFileMarks
//
//  The files in a listed receiving folder that arrived since the folder was
//  last listed. The set is in memory for that listing only: listing another
//  location clears it.
//
////////////////////////////////////////////////////////////////////////////////

class NewFileMarks
{
public:
    struct File
    {
        std::wstring  name;
        long long     written = 0;
    };

    void  Clear   ();
    void  AddNew  (const std::wstring & folder, long long seen, const std::vector<File> & files);
    bool  IsNew   (const std::wstring & folder, const std::wstring & name) const;
    bool  IsEmpty () const { return m_names.empty(); }

private:
    static std::wstring  ToKey (const std::wstring & text);

    std::wstring            m_folder;
    std::set<std::wstring>  m_names;
};
