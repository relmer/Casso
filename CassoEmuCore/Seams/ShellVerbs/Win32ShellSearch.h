#pragma once

#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellVerbsTypes.h"
#include "Seams/IShellItemVerbs.h"
#include "Seams/Win32ShellPreview.h"

class Win32ShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellSearch
//
//  Search: the Windows Search index queried where a folder is indexed,
//  and a walk of the folders where it is not, for matching files and disk
//  images.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellSearch
{
public:
    explicit Win32ShellSearch (Win32ShellItemVerbs & verbs);

    //  A search stops at this many results, as a folder too large to read
    //  whole would otherwise never finish.
    static constexpr size_t  s_kMaxResults = 5000;

    static HRESULT  RunSearch     (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static bool     IsIndexed     (const std::wstring & scope);
    static HRESULT  QueryIndex    (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths);
    static void     WalkForNames  (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths);

    //  The disk images a search reads, at most, so a folder of thousands
    //  cannot keep it running for minutes.
    static constexpr size_t  s_kMaxImages           = 2000;
    static constexpr size_t  s_kMaxImageDirectories = 256;   // in one image, so a looping catalog cannot run on

    static void     SearchImages  (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    static void     WalkForImages (const std::wstring & scope, std::vector<std::wstring> & outImages);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;
};
