#include "Pch.h"

#include "CassoExplorer/Model/NewFileMarks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NewFileMarks::Clear
//
////////////////////////////////////////////////////////////////////////////////

void NewFileMarks::Clear()
{
    m_folder.clear();
    m_names.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NewFileMarks::AddNew
//
//  Adds the files written after `seen`. A folder other than the one marked
//  starts a new set.
//
////////////////////////////////////////////////////////////////////////////////

void NewFileMarks::AddNew (const std::wstring & folder, long long seen, const std::vector<File> & files)
{
    std::wstring  key = ToKey (folder);



    if (key != m_folder)
    {
        Clear();
        m_folder = key;
    }

    for (const File & file : files)
    {
        if (file.written > seen)
        {
            m_names.insert (ToKey (file.name));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NewFileMarks::IsNew
//
////////////////////////////////////////////////////////////////////////////////

bool NewFileMarks::IsNew (const std::wstring & folder, const std::wstring & name) const
{
    return ToKey (folder) == m_folder && m_names.contains (ToKey (name));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NewFileMarks::ToKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NewFileMarks::ToKey (const std::wstring & text)
{
    std::wstring  key = text;



    std::transform (key.begin(), key.end(), key.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

    return key;
}
