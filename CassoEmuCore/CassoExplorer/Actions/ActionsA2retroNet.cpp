#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/A2retroNetConfig.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::GetConfigPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsA2retroNet::GetConfigPath (const std::wstring & imagePath)
{
    std::filesystem::path  root = std::filesystem::path (imagePath).root_path();



    return (root / A2retroNetConfig::kpszFileName).wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::CanAdd
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsA2retroNet::CanAdd (const std::wstring & imagePath) const
{
    return !imagePath.empty() && m_actions.m_fs.Exists (GetConfigPath (imagePath));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::Add
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::Add (const std::wstring & imagePath)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io       = m_actions.m_browser.GetOperations().GetFileIo();
    std::wstring           leaf     = CassoExplorerActions::GetLeafName (imagePath);
    std::string            config   = TextEncoding::WideToNarrow (GetConfigPath (imagePath));
    std::vector<Byte>      image;
    std::vector<Byte>      text;
    std::string            problem;
    std::string            path;
    std::string            written;
    A2retroNetConfig       settings;
    int                    drive    = 0;
    bool                   isSet    = false;
    HRESULT                hr       = S_OK;



    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (imagePath), image);

    if (SUCCEEDED (hr))
    {
        hr = io.ReadAllBytes (config, text);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, leaf + L" or A2retroNET.txt could not be read.");
        return outcome;
    }

    if (!A2retroNetConfig::CheckImage (TextEncoding::WideToNarrow (imagePath), image, problem))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), std::format (L"{} wasn't added. {}", leaf, TextEncoding::NarrowToWide (problem)));
        return outcome;
    }

    settings = A2retroNetConfig::Parse (std::string (text.begin(), text.end()));
    path     = A2retroNetConfig::MakePath (std::filesystem::path (imagePath).root_path(), imagePath);

    for (int i = 0; !isSet && i < settings.number; i++)
    {
        isSet = _stricmp (settings.paths[i].c_str(), path.c_str()) == 0;
        drive = isSet ? i : drive;
    }

    if (isSet)
    {
        ActionsCopy::AppendMessage (outcome, S_OK, std::format (L"{} is drive {} already.", leaf, drive + 1));
        return outcome;
    }

    if (!settings.TryFindFreeDrive (drive))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_DISK_FULL), std::format (L"Each of the {} drives has an image, so {} wasn't added.", settings.number, leaf));
        return outcome;
    }

    settings.paths[drive] = path;
    written               = settings.Write();

    text.assign (written.begin(), written.end());

    hr = io.WriteAllBytes (config, text);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"A2retroNET.txt could not be written.");
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, std::format (L"{} is drive {}.", leaf, drive + 1));
    outcome.written = 1;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::CreateConfig
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::CreateConfig (const std::wstring & root)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io     = m_actions.m_browser.GetOperations().GetFileIo();
    std::wstring           path   = (std::filesystem::path (root) / A2retroNetConfig::kpszFileName).wstring();
    std::string            text   = A2retroNetConfig().Write();
    std::vector<Byte>      bytes (text.begin(), text.end());
    HRESULT                hr     = S_OK;



    if (m_actions.m_fs.Exists (path))
    {
        ActionsCopy::AppendMessage (outcome, S_OK, L"The card has an A2retroNET.txt already.");
        return outcome;
    }

    hr = io.WriteAllBytes (TextEncoding::WideToNarrow (path), bytes);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"A2retroNET.txt could not be written.");
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, L"The card has an A2retroNET.txt with no drives set.");
    outcome.written = 1;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::IsCard
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsA2retroNet::IsCard (const std::wstring & folder) const
{
    std::filesystem::path  path (folder);



    return !folder.empty() && path.root_path() == path && m_actions.m_fs.Exists (GetConfigPath (folder));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::ReadCard
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsA2retroNet::ReadCard (const std::wstring & root, Card & outCard)
{
    HRESULT              hr = S_OK;
    IDiskFileIo        & io = m_actions.m_browser.GetOperations().GetFileIo();
    std::vector<Byte>    text;



    outCard      = Card();
    outCard.root = root;

    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (GetConfigPath (root)), text);
    CHR (hr);

    outCard.config = A2retroNetConfig::Parse (std::string (text.begin(), text.end()));

    for (int index = 0; index < outCard.config.number; index++)
    {
        outCard.drives.push_back (CheckDrive (root, index + 1, outCard.config.paths[index]));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::CheckDrive
//
//  The image is checked from its size and its first bytes, so a card of
//  32 MB images is listed without reading them.
//
////////////////////////////////////////////////////////////////////////////////

ActionsA2retroNet::Drive ActionsA2retroNet::CheckDrive (const std::wstring & root, int number, const std::string & path)
{
    Drive               drive;
    IDiskFileIo       & io         = m_actions.m_browser.GetOperations().GetFileIo();
    HRESULT             hrStat     = S_OK;
    HRESULT             hrRead     = S_OK;
    HRESULT             hrReadOnly = S_OK;
    std::string         relative;
    std::string         problem;
    std::wstring        host;
    std::vector<Byte>   header;
    FileStamp           stamp;
    bool                hasDevice  = false;
    bool                readOnly   = false;
    bool                isReadable = false;



    drive.number = number;
    drive.path   = path;

    if (path.empty())
    {
        return drive;
    }

    hasDevice       = A2retroNetConfig::SplitDevice (path, relative);
    host            = (std::filesystem::path (root) / std::filesystem::path (TextEncoding::NarrowToWide (relative)).make_preferred()).wstring();
    hrStat          = io.Stat (TextEncoding::WideToNarrow (host), stamp);
    drive.isPresent = SUCCEEDED (hrStat);

    if (!drive.isPresent)
    {
        drive.problem = hasDevice ? std::format (L"{} isn't on this card. It may be on the other storage device.", TextEncoding::NarrowToWide (path))
                                  : std::format (L"{} isn't on the card.", TextEncoding::NarrowToWide (path));
        return drive;
    }

    header.resize ((size_t) std::min<uint64_t> (stamp.sizeBytes, A2retroNetConfig::kTwoImgHeader));

    drive.bytes = stamp.sizeBytes;
    hrRead      = io.ReadAt (TextEncoding::WideToNarrow (host), 0, header);
    isReadable  = SUCCEEDED (hrRead) && A2retroNetConfig::CheckImage (relative, header, stamp.sizeBytes, problem);

    if (!isReadable)
    {
        drive.problem = problem.empty() ? std::format (L"{} could not be read.", TextEncoding::NarrowToWide (path)) : TextEncoding::NarrowToWide (problem);
    }

    hrReadOnly             = m_actions.m_fs.GetReadOnlyAttribute (host, readOnly);
    drive.isWriteProtected = SUCCEEDED (hrReadOnly) && readOnly;

    return drive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::GetConfigStamp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsA2retroNet::GetConfigStamp (const std::wstring & root, FileStamp & outStamp)
{
    return m_actions.m_browser.GetOperations().GetFileIo().Stat (TextEncoding::WideToNarrow (GetConfigPath (root)), outStamp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::Repair
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::Repair (const std::wstring & root)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io      = m_actions.m_browser.GetOperations().GetFileIo();
    std::string            config  = TextEncoding::WideToNarrow (GetConfigPath (root));
    std::vector<Byte>      text;
    std::string            written;
    HRESULT                hr      = S_OK;



    hr = io.ReadAllBytes (config, text);

    if (SUCCEEDED (hr))
    {
        written = A2retroNetConfig::Parse (std::string (text.begin(), text.end())).Write();
        text.assign (written.begin(), written.end());
        hr      = io.WriteAllBytes (config, text);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"A2retroNET.txt could not be repaired.");
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, L"A2retroNET.txt has LF line ends.");
    outcome.written = 1;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::Summarize
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsA2retroNet::Summarize (const Card & card)
{
    int           set  = 0;
    std::wstring  text;



    for (const Drive & drive : card.drives)
    {
        set += drive.path.empty() ? 0 : 1;
    }

    text = std::format (L"A2Pico card. {} of its {} A2retroNET drives have an image.", set, card.drives.size());

    if (card.config.hasCarriageReturns)
    {
        return text + L" A2retroNET.txt has CR LF line ends, so the A2Pico doesn't open any image. Repair writes it with LF line ends.";
    }

    for (const Drive & drive : card.drives)
    {
        if (!drive.problem.empty())
        {
            text += std::format (L" Drive {}: {}", drive.number, drive.problem);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::Save
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::Save (const std::wstring & root, const A2retroNetConfig & config)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io      = m_actions.m_browser.GetOperations().GetFileIo();
    std::string            written = config.Write();
    std::vector<Byte>      text (written.begin(), written.end());
    HRESULT                hr      = S_OK;



    hr = io.WriteAllBytes (TextEncoding::WideToNarrow (GetConfigPath (root)), text);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"A2retroNET.txt could not be written.");
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, L"A2retroNET.txt is saved.");
    outcome.written = 1;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::ListImages
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsA2retroNet::ListImages (const std::wstring & root, std::vector<std::string> & outPaths)
{
    HRESULT  hr = S_OK;



    outPaths.clear();

    hr = ListImagesIn (root, std::string(), outPaths);
    CHR (hr);

    std::sort (outPaths.begin(), outPaths.end(), [] (const std::string & a, const std::string & b) { return _stricmp (a.c_str(), b.c_str()) < 0; });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::ListImagesIn
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsA2retroNet::ListImagesIn (const std::wstring & folder, const std::string & prefix, std::vector<std::string> & ioPaths)
{
    HRESULT                       hr = S_OK;
    std::vector<FileSystemEntry>  entries;
    std::string                   name;



    hr = m_actions.m_fs.EnumerateEntries (folder, entries);
    CHR (hr);

    for (const FileSystemEntry & entry : entries)
    {
        BAIL_OUT_IF (ioPaths.size() >= kImagesMax, S_OK);

        name = prefix + TextEncoding::WideToNarrow (entry.name);

        if (entry.isFolder)
        {
            hr = ListImagesIn ((std::filesystem::path (folder) / entry.name).wstring(), name + "/", ioPaths);
            CHR (hr);
        }
        else if (A2retroNetConfig::IsImageName (name))
        {
            ioPaths.push_back (name);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::TryReadConfigFor
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsA2retroNet::TryReadConfigFor (const std::wstring & path, A2retroNetConfig & outConfig)
{
    IDiskFileIo        & io     = m_actions.m_browser.GetOperations().GetFileIo();
    std::wstring         config = GetConfigPath (path);
    std::vector<Byte>    text;
    HRESULT              hr     = S_OK;



    if (path.empty() || !m_actions.m_fs.Exists (config))
    {
        return false;
    }

    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (config), text);

    if (FAILED (hr))
    {
        return false;
    }

    outConfig = A2retroNetConfig::Parse (std::string (text.begin(), text.end()));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::NoteMove
//
////////////////////////////////////////////////////////////////////////////////

void ActionsA2retroNet::NoteMove (const std::wstring & from, const std::wstring & to)
{
    A2retroNetConfig       config;
    std::filesystem::path  root     = std::filesystem::path (from).root_path();
    std::string            relative;
    bool                   changed  = false;
    bool                   sameCard = false;



    if (!TryReadConfigFor (from, config))
    {
        return;
    }

    relative = A2retroNetConfig::MakePath (root, from);
    sameCard = _wcsicmp (root.wstring().c_str(), std::filesystem::path (to).root_path().wstring().c_str()) == 0;
    changed  = sameCard ? config.MovePath (relative, A2retroNetConfig::MakePath (root, to)) : config.ClearPath (relative);

    if (changed)
    {
        Save (root.wstring(), config);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::NoteDelete
//
////////////////////////////////////////////////////////////////////////////////

void ActionsA2retroNet::NoteDelete (const std::wstring & path)
{
    A2retroNetConfig       config;
    std::filesystem::path  root = std::filesystem::path (path).root_path();



    if (TryReadConfigFor (path, config) && config.ClearPath (A2retroNetConfig::MakePath (root, path)))
    {
        Save (root.wstring(), config);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::DescribeDrives
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsA2retroNet::DescribeDrives (const std::vector<std::wstring> & paths)
{
    std::wstring  text;
    size_t        files = 0;



    for (const std::wstring & path : paths)
    {
        A2retroNetConfig  config;
        std::vector<int>  drives;
        std::wstring      numbers;



        if (!TryReadConfigFor (path, config))
        {
            continue;
        }

        drives = config.FindDrives (A2retroNetConfig::MakePath (std::filesystem::path (path).root_path(), path));

        for (size_t i = 0; i < drives.size(); i++)
        {
            numbers += std::format (L"{}{}", i == 0 ? L"" : i + 1 == drives.size() ? L" and " : L", ", drives[i] + 1);
        }

        if (!drives.empty())
        {
            text += std::format (L"{}{} is drive{} {} of this A2Pico card.", text.empty() ? L"" : L" ", CassoExplorerActions::GetLeafName (path), drives.size() == 1 ? L"" : L"s", numbers);
            files++;
        }
    }

    if (!text.empty())
    {
        text += files == 1 ? L" Deleting it empties the drive." : L" Deleting them empties those drives.";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::ReadLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ActionsA2retroNet::ReadLong (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) bytes[at] | ((uint32_t) bytes[at + 1] << 8) | ((uint32_t) bytes[at + 2] << 16) | ((uint32_t) bytes[at + 3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::TryGetConvertibleLayout
//
//  A .2mg in ProDOS order whose blocks start elsewhere than byte 64 is
//  converted without looking further. A 140K volume in DOS order is
//  converted when its block 2 is a ProDOS volume directory's key block.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsA2retroNet::TryGetConvertibleLayout (const std::wstring & path, Layout & outLayout)
{
    IDiskFileIo        & io        = m_actions.m_browser.GetOperations().GetFileIo();
    std::string          narrow    = TextEncoding::WideToNarrow (path);
    std::wstring         extension = std::filesystem::path (path).extension().wstring();
    std::vector<Byte>    header (A2retroNetConfig::kTwoImgHeader);
    std::vector<Byte>    key (ProDosSkeleton::kBlockByteSize / 2);
    FileStamp            stamp;
    HRESULT              hr        = S_OK;
    bool                 isTwoImg  = false;
    uint32_t             format    = 0;



    outLayout = Layout();

    std::transform (extension.begin(), extension.end(), extension.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

    isTwoImg = extension == L".2mg";

    if (!isTwoImg && extension != L".dsk" && extension != L".do")
    {
        return false;
    }

    hr = io.Stat (narrow, stamp);

    if (FAILED (hr))
    {
        return false;
    }

    outLayout.length     = stamp.sizeBytes;
    outLayout.isDosOrder = true;

    if (isTwoImg)
    {
        hr = stamp.sizeBytes >= header.size() ? io.ReadAt (narrow, 0, header) : HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF);

        if (FAILED (hr))
        {
            return false;
        }

        format               = ReadLong (header, k2mgFormatAt);
        outLayout.start      = ReadLong (header, k2mgOffsetAt);
        outLayout.length     = ReadLong (header, k2mgLengthAt);
        outLayout.isDosOrder = format == k2mgDosOrder;

        if (format == k2mgProDos)
        {
            return outLayout.start != A2retroNetConfig::kTwoImgHeader && outLayout.start + outLayout.length <= stamp.sizeBytes && outLayout.length >= A2retroNetConfig::kBlockBytes;
        }
    }

    if (!outLayout.isDosOrder || outLayout.length != (uint64_t) NibblizationLayer::kImageByteSize || outLayout.start + outLayout.length > stamp.sizeBytes)
    {
        return false;
    }

    hr = io.ReadAt (narrow, outLayout.start + ProDosSkeleton::GetBlockByteOffset (kVolumeKeyBlock, 0), key);

    return SUCCEEDED (hr) && key[0] == 0 && key[1] == 0 && (key[kStorageTypeAt] & kVolumeHeaderType) == kVolumeHeaderType;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::FindConvertible
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsA2retroNet::FindConvertible (const std::wstring & folder, std::vector<std::wstring> & outPaths)
{
    HRESULT                       hr = S_OK;
    std::vector<FileSystemEntry>  entries;
    Layout                        layout;



    outPaths.clear();

    hr = m_actions.m_fs.EnumerateEntries (folder, entries);
    CHR (hr);

    for (const FileSystemEntry & entry : entries)
    {
        std::wstring  po        = std::filesystem::path (entry.name).replace_extension (L".po").wstring();
        bool          hasPo     = std::any_of (entries.begin(), entries.end(), [&po] (const FileSystemEntry & each) { return _wcsicmp (each.name.c_str(), po.c_str()) == 0; });
        std::wstring  path      = (std::filesystem::path (folder) / entry.name).wstring();



        if (!entry.isFolder && !hasPo && TryGetConvertibleLayout (path, layout))
        {
            outPaths.push_back (path);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsA2retroNet::ConvertToPo
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsA2retroNet::ConvertToPo (const std::wstring & path, std::wstring & outPath)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io     = m_actions.m_browser.GetOperations().GetFileIo();
    std::wstring           leaf   = CassoExplorerActions::GetLeafName (path);
    std::vector<Byte>      bytes;
    std::vector<Byte>      blocks;
    Layout                 layout;
    HRESULT                hr     = S_OK;



    outPath = std::filesystem::path (path).replace_extension (L".po").wstring();

    if (!TryGetConvertibleLayout (path, layout))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), std::format (L"{} has no ProDOS volume to write as a .po.", leaf));
        return outcome;
    }

    if (m_actions.m_fs.Exists (outPath))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS), std::format (L"{} is there already.", CassoExplorerActions::GetLeafName (outPath)));
        return outcome;
    }

    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (path), bytes);

    if (SUCCEEDED (hr) && layout.start + layout.length > bytes.size())
    {
        hr = HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF);
    }

    if (SUCCEEDED (hr))
    {
        blocks.assign (bytes.begin() + (ptrdiff_t) layout.start, bytes.begin() + (ptrdiff_t) (layout.start + layout.length));

        //  A DOS-order image has each block's halves in two sectors of its
        //  track.
        for (size_t at = 0; layout.isDosOrder && at < blocks.size(); at++)
        {
            blocks[at] = bytes[layout.start + ProDosSkeleton::GetBlockByteOffset ((int) (at / ProDosSkeleton::kBlockByteSize), at % ProDosSkeleton::kBlockByteSize)];
        }

        hr = io.WriteAllBytes (TextEncoding::WideToNarrow (outPath), blocks);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, std::format (L"{} could not be written as a .po.", leaf));
        return outcome;
    }

    ActionsCopy::AppendMessage (outcome, S_OK, std::format (L"{} is written as {}.", leaf, CassoExplorerActions::GetLeafName (outPath)));
    outcome.written = 1;

    return outcome;
}
