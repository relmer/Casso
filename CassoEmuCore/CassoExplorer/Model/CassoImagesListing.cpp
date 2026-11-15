#include "Pch.h"

#include "CassoExplorer/Model/CassoImagesListing.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/IFileSystem.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/ImageClassCache.h"
#include "Devices/Disk/ImageClassifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoImagesListing::Build
//
//  Rows are listed as a search's results are: the full path to open, and
//  the folder for the Folder column. The order is the caller's sort.
//
////////////////////////////////////////////////////////////////////////////////

void CassoImagesListing::Build (
    IFileSystem                        & fs,
    IDiskFileIo                        & fileIo,
    ImageClassCache                    & cache,
    const std::vector<std::wstring>    & folders,
    const std::vector<DiskMru::Entry>  & recent,
    std::vector<CatalogRow>            & outRows)
{
    std::unordered_map<std::wstring, int64_t>  loadedAt;
    std::set<std::wstring>                     listed;
    std::vector<FileSystemEntry>               entries;
    HRESULT                                    hr        = S_OK;



    outRows.clear();

    for (const DiskMru::Entry & entry : recent)
    {
        loadedAt.emplace (MakePathKey (entry.path.wstring()), entry.lastLoadedUnix);
    }

    for (const std::wstring & folder : folders)
    {
        entries.clear();

        hr = fs.EnumerateAllEntries (folder, entries);

        if (FAILED (hr))
        {
            continue;
        }

        for (const FileSystemEntry & entry : entries)
        {
            std::wstring  path   = (std::filesystem::path (folder) / entry.name).wstring();
            std::wstring  key    = MakePathKey (path);
            bool          isNew  = false;
            CatalogRow    row;



            if (entry.isFolder || !ImageClassifier::IsImageExtension (std::filesystem::path (entry.name).extension().wstring()))
            {
                continue;
            }

            isNew = listed.insert (key).second;

            if (!isNew)
            {
                continue;
            }

            row              = CatalogModel::FromHostEntry (entry, TreeModel::IsSupportedImage (entry.name), outRows.size());
            row.hostPath     = path;
            row.folderPath   = folder;
            row.imageClass   = cache.Get (fileIo, TextEncoding::WideToUtf8 (path), entry.sizeBytes, entry.modifiedUnix);
            row.lastUsedUnix = loadedAt.contains (key) ? loadedAt.at (key) : 0;

            outRows.push_back (std::move (row));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoImagesListing::MakePathKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoImagesListing::MakePathKey (const std::wstring & path)
{
    std::wstring  key = std::filesystem::path (path).lexically_normal().wstring();



    for (wchar_t & c : key)
    {
        c = (c == L'/') ? L'\\' : (wchar_t) towlower (c);
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoImagesListing::LoadRecent
//
//  Read at each listing, since the emulator adds loads while Casso Explorer
//  is open.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskMru::Entry> CassoImagesListing::LoadRecent (IFileSystem & fs, const std::wstring & baseDir)
{
    GlobalUserPrefs  prefs;
    HRESULT          hr = prefs.Load (baseDir, fs);



    IGNORE_RETURN_VALUE (hr, S_OK);

    return DiskMru::FromUtf8 (prefs.recentDisks, prefs.recentDiskLoadedAt).GetSnapshot();
}
