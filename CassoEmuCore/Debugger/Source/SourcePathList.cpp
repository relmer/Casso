#include "Pch.h"

#include "Debugger/Source/SourcePathList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::GetProgramFolders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SourcePathList::GetProgramFolders (const std::string & programKey) const
{
    auto  found = m_prefs.debuggerProgramSourceFolders.find (programKey);



    return (found == m_prefs.debuggerProgramSourceFolders.end()) ? std::vector<std::wstring>() : ToWide (found->second);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::GetGlobalFolders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SourcePathList::GetGlobalFolders() const
{
    return ToWide (m_prefs.debuggerSourceFolders);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::AddFound
//
////////////////////////////////////////////////////////////////////////////////

void SourcePathList::AddFound (const std::string & programKey, const std::wstring & folder)
{
    std::string  utf8 = WideToUtf8 (folder);



    if (utf8.empty())
    {
        return;
    }

    PushFront (m_prefs.debuggerSourceFolders, utf8);

    if (!programKey.empty())
    {
        PushFront (m_prefs.debuggerProgramSourceFolders[programKey], utf8);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::PushFront
//
////////////////////////////////////////////////////////////////////////////////

void SourcePathList::PushFront (std::vector<std::string> & list, const std::string & folder)
{
    std::wstring  wide = Utf8ToWide (folder);



    std::erase_if (list, [&] (const std::string & each) { return IsSameFolder (Utf8ToWide (each), wide); });
    list.insert (list.begin(), folder);

    if (list.size() > kMaxFolders)
    {
        list.resize (kMaxFolders);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::ToWide
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SourcePathList::ToWide (const std::vector<std::string> & list)
{
    std::vector<std::wstring>  wide;



    for (const std::string & each : list)
    {
        wide.push_back (Utf8ToWide (each));
    }

    return wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::IsSameFolder
//
//  Case and separators aside, and a trailing separator ignored.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePathList::IsSameFolder (const std::wstring & a, const std::wstring & b)
{
    auto  normalize = [] (const std::wstring & path)
                      {
                          std::wstring  result;

                          for (wchar_t c : path)
                          {
                              result += (c == L'/') ? L'\\' : (wchar_t) towlower (c);
                          }

                          while (!result.empty() && result.back() == L'\\')
                          {
                              result.pop_back();
                          }

                          return result;
                      };



    return normalize (a) == normalize (b);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::Utf8ToWide
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourcePathList::Utf8ToWide (const std::string & text)
{
    int           length = 0;
    std::wstring  wide;



    if (text.empty())
    {
        return wide;
    }

    length = MultiByteToWideChar (CP_UTF8, 0, text.data(), (int) text.size(), nullptr, 0);
    wide.resize ((size_t) length);
    MultiByteToWideChar (CP_UTF8, 0, text.data(), (int) text.size(), wide.data(), length);

    return wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList::WideToUtf8
//
////////////////////////////////////////////////////////////////////////////////

std::string SourcePathList::WideToUtf8 (const std::wstring & text)
{
    int          length = 0;
    std::string  utf8;



    if (text.empty())
    {
        return utf8;
    }

    length = WideCharToMultiByte (CP_UTF8, 0, text.data(), (int) text.size(), nullptr, 0, nullptr, nullptr);
    utf8.resize ((size_t) length);
    WideCharToMultiByte (CP_UTF8, 0, text.data(), (int) text.size(), utf8.data(), length, nullptr, nullptr);

    return utf8;
}
