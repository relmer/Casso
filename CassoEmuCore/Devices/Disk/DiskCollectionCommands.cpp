#include "Pch.h"

#include "Devices/Disk/DiskCollectionCommands.h"
#include "CassoExplorer/Model/CollectionResults.h"
#include "CassoExplorer/Model/CollectionWalk.h"
#include "CassoExplorer/Model/ImageFileSearchSources.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::Run
//
////////////////////////////////////////////////////////////////////////////////

void DiskCollectionCommands::Run (IDiskFileIo & io, const CommandLineOptions & options, DiskCommandResult & result)
{
    const std::string  & folder = options.disk.imagePath;



    if (!io.IsFolder (folder))
    {
        result.diagnostics += "Error: " + folder + " is not a folder\n";
        result.exitStatus   = DiskCommandResult::kNoOutput;
        return;
    }

    if (options.disk.command == CommandLineOptions::DiskOptions::Command::Search)
    {
        RunSearch (io, folder, options.disk.path, result);
    }
    else
    {
        RunDupes (io, folder, result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::RunList
//
////////////////////////////////////////////////////////////////////////////////

void DiskCollectionCommands::RunList (IDiskFileIo & io, const std::string & folder, DiskCommandResult & result)
{
    std::vector<CollectionDisk>  disks;
    HRESULT                      hr = S_OK;



    hr = Collect (io, folder, disks, result);

    if (SUCCEEDED (hr))
    {
        result.output += CollectionResults::FormatDiskList (disks);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::RunDupes
//
////////////////////////////////////////////////////////////////////////////////

void DiskCollectionCommands::RunDupes (IDiskFileIo & io, const std::string & folder, DiskCommandResult & result)
{
    std::vector<CollectionDisk>  disks;
    std::vector<DuplicateSet>    sets;
    HRESULT                      hr = S_OK;



    hr = Collect (io, folder, disks, result);

    if (SUCCEEDED (hr))
    {
        CollectionResults::FindDuplicates (disks, sets);
        result.output += CollectionResults::FormatDuplicates (disks, sets);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::RunSearch
//
//  The words are matched without regard to case, as the window's search
//  matches them.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCollectionCommands::RunSearch (IDiskFileIo & io, const std::string & folder, const std::string & text, DiskCommandResult & result)
{
    std::wstring              lowerText = TextEncoding::NarrowToWide (text);
    CollectionWalk::Visitor   visitor;
    HRESULT                   hr        = S_OK;



    std::transform (lowerText.begin(), lowerText.end(), lowerText.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

    visitor.onFile = [&lowerText, &result] (const CollectionDisk & disk, const std::string & path, const FileEntry & entry, VolumeKind kind, const FilePayload & payload)
    {
        std::wstring  fileText;

        if (ImageFileSearchSources::GetPayloadText (entry, kind, payload, fileText) && fileText.find (lowerText) != std::wstring::npos)
        {
            result.output += GetPrintable (disk.path + ContainerPath::kPathSeparator + path) + "\n";
        }
    };

    hr = CollectionWalk::Walk (io, folder, visitor);

    if (FAILED (hr))
    {
        ReportUnreadable (folder, result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::Collect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCollectionCommands::Collect (IDiskFileIo & io, const std::string & folder, std::vector<CollectionDisk> & outDisks, DiskCommandResult & result)
{
    HRESULT                  hr = S_OK;
    CollectionWalk::Visitor  visitor;



    outDisks.clear();

    visitor.onDisk = [&outDisks] (CollectionDisk && disk) { outDisks.push_back (std::move (disk)); };

    hr = CollectionWalk::Walk (io, folder, visitor);

    if (FAILED (hr))
    {
        ReportUnreadable (folder, result);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::ReportUnreadable
//
////////////////////////////////////////////////////////////////////////////////

void DiskCollectionCommands::ReportUnreadable (const std::string & folder, DiskCommandResult & result)
{
    result.diagnostics += "Error: " + folder + ": the folder could not be read\n";
    result.exitStatus   = DiskCommandResult::kNoOutput;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands::GetPrintable
//
//  A path with each control character in caret form, as a catalog lists a
//  DOS 3.3 name: ^H for a backspace, ^? for DEL.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCollectionCommands::GetPrintable (const std::string & path)
{
    std::string  printable;



    for (char c : path)
    {
        Byte  b = (Byte) c;

        if (b < 0x20)
        {
            printable += '^';
            printable += (char) (b + 0x40);
        }
        else if (b == 0x7F)
        {
            printable += "^?";
        }
        else
        {
            printable += c;
        }
    }

    return printable;
}