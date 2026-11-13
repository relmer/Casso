#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/A2retroNetConfig.h"





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
//  Each image is checked from its size and its first bytes, so a card of
//  32 MB images is listed without reading them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsA2retroNet::ReadCard (const std::wstring & root, Card & outCard)
{
    HRESULT              hr         = S_OK;
    HRESULT              hrStat     = S_OK;
    HRESULT              hrRead     = S_OK;
    HRESULT              hrReadOnly = S_OK;
    IDiskFileIo        & io         = m_actions.m_browser.GetOperations().GetFileIo();
    std::vector<Byte>    text;
    std::string          relative;
    std::string          problem;
    std::wstring         host;
    FileStamp            stamp;
    bool                 hasDevice  = false;
    bool                 readOnly   = false;
    bool                 isReadable = false;



    outCard      = Card();
    outCard.root = root;

    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (GetConfigPath (root)), text);
    CHR (hr);

    outCard.config = A2retroNetConfig::Parse (std::string (text.begin(), text.end()));

    for (int index = 0; index < outCard.config.number; index++)
    {
        Drive  drive;



        drive.number = index + 1;
        drive.path   = outCard.config.paths[index];

        if (!drive.path.empty())
        {
            hasDevice       = A2retroNetConfig::SplitDevice (drive.path, relative);
            host            = (std::filesystem::path (root) / std::filesystem::path (TextEncoding::NarrowToWide (relative)).make_preferred()).wstring();
            hrStat          = io.Stat (TextEncoding::WideToNarrow (host), stamp);
            drive.isPresent = SUCCEEDED (hrStat);

            if (drive.isPresent)
            {
                std::vector<Byte>  header ((size_t) std::min<uint64_t> (stamp.sizeBytes, A2retroNetConfig::kTwoImgHeader));

                drive.bytes = stamp.sizeBytes;
                problem.clear();

                hrRead     = io.ReadAt (TextEncoding::WideToNarrow (host), 0, header);
                isReadable = SUCCEEDED (hrRead) && A2retroNetConfig::CheckImage (relative, header, stamp.sizeBytes, problem);

                if (!isReadable)
                {
                    drive.problem = problem.empty() ? std::format (L"{} could not be read.", TextEncoding::NarrowToWide (drive.path)) : TextEncoding::NarrowToWide (problem);
                }

                hrReadOnly             = m_actions.m_fs.GetReadOnlyAttribute (host, readOnly);
                drive.isWriteProtected = SUCCEEDED (hrReadOnly) && readOnly;
            }
            else
            {
                drive.problem = hasDevice ? std::format (L"{} isn't on this card. It may be on the other storage device.", TextEncoding::NarrowToWide (drive.path))
                                          : std::format (L"{} isn't on the card.", TextEncoding::NarrowToWide (drive.path));
            }
        }

        outCard.drives.push_back (std::move (drive));
    }

Error:
    return hr;
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
