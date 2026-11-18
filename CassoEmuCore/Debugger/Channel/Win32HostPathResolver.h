#pragma once

#include "Debugger/Channel/IHostPathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostPathResolver
//
//  IHostPathResolver over the real file system: GetFinalPathNameByHandle on
//  a handle opened for no access, and SHGetKnownFolderPath.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostPathResolver : public IHostPathResolver
{
public:
    bool  TryGetFinalPath   (const std::wstring & path, std::wstring & finalPath) override;
    bool  Exists            (const std::wstring & path) override;
    bool  TryGetKnownFolder (HostKnownFolder folder, std::wstring & path) override;

    //  `\\?\C:\x` to `C:\x` and `\\?\UNC\server\share` to `\\server\share`.
    static std::wstring  RemoveLongPathPrefix (const std::wstring & path);
};
