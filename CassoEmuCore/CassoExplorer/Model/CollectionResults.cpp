#include "Pch.h"

#include "CassoExplorer/Model/CollectionResults.h"
#include "CassoExplorer/Model/DiskIdentity.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResults::FindDuplicates
//
////////////////////////////////////////////////////////////////////////////////

void CollectionResults::FindDuplicates (const std::vector<CollectionDisk> & disks, std::vector<DuplicateSet> & outSets)
{
    std::map<std::string, std::vector<size_t>>         byVolume;
    std::map<std::string, std::vector<DuplicateMember>> byFile;
    std::map<std::string, Byte>                         fileTypes;
    std::vector<DuplicateSet>                           fileSets;



    outSets.clear();

    for (size_t at = 0; at < disks.size(); at++)
    {
        if (!disks[at].sha256.empty())
        {
            byVolume[disks[at].sha256].push_back (at);
        }

        for (const CollectionFile & file : disks[at].fileList)
        {
            std::string  key = DiskIdentity::GetFileKey (file.sha256, file.type);

            if (file.bytes == 0 || file.sha256.empty())
            {
                continue;
            }

            byFile[key].push_back ({ at, file.path });
            fileTypes[key] = file.type;
        }
    }

    for (const auto & [sha256, members] : byVolume)
    {
        DuplicateSet  set;

        if (members.size() < 2)
        {
            continue;
        }

        set.sha256 = sha256;

        for (size_t disk : members)
        {
            set.members.push_back ({ disk, std::string() });
        }

        outSets.push_back (std::move (set));
    }

    for (auto & [key, members] : byFile)
    {
        std::set<std::string>  volumes;
        DuplicateSet           set;

        for (const DuplicateMember & member : members)
        {
            volumes.insert (disks[member.disk].sha256);
        }

        if (volumes.size() < 2)
        {
            continue;
        }

        set.isFile  = true;
        set.sha256  = key.substr (0, key.find (':'));
        set.type    = fileTypes[key];
        set.members = std::move (members);

        fileSets.push_back (std::move (set));
    }

    std::sort (outSets.begin(),  outSets.end(),  [&disks] (const DuplicateSet & a, const DuplicateSet & b) { return GetSortKey (disks, a) < GetSortKey (disks, b); });
    std::sort (fileSets.begin(), fileSets.end(), [&disks] (const DuplicateSet & a, const DuplicateSet & b) { return GetSortKey (disks, a) < GetSortKey (disks, b); });

    for (DuplicateSet & set : fileSets)
    {
        outSets.push_back (std::move (set));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResults::FormatDiskList
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionResults::FormatDiskList (const std::vector<CollectionDisk> & disks)
{
    std::string  text = std::string ("path,container,fileSystem,volumeName,files,freeBytes,crc32,sha256,hasMore") + kLineEnd;



    for (const CollectionDisk & disk : disks)
    {
        text += std::format ("{},{},{},{},{},{},{},{},{}{}",
                             QuoteField (disk.path), QuoteField (disk.container), QuoteField (disk.fileSystem), QuoteField (disk.volumeName),
                             disk.files, disk.freeBytes, DiskIdentity::FormatCrc32 (disk.crc32), disk.sha256, disk.hasMore ? "yes" : "no", kLineEnd);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResults::FormatDuplicates
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionResults::FormatDuplicates (const std::vector<CollectionDisk> & disks, const std::vector<DuplicateSet> & sets)
{
    std::string  text = std::string ("set,kind,path,name,sha256") + kLineEnd;



    for (size_t at = 0; at < sets.size(); at++)
    {
        for (const DuplicateMember & member : sets[at].members)
        {
            text += std::format ("{},{},{},{},{}{}", at + 1, sets[at].isFile ? "file" : "disk",
                                 QuoteField (disks[member.disk].path), QuoteField (member.file), sets[at].sha256, kLineEnd);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResults::QuoteField
//
//  A field with a comma, a quote or a line break in quotes, each quote in
//  it doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionResults::QuoteField (const std::string & field)
{
    std::string  quoted = "\"";



    if (field.find_first_of (",\"\r\n") == std::string::npos)
    {
        return field;
    }

    for (char c : field)
    {
        quoted += c;

        if (c == '"')
        {
            quoted += '"';
        }
    }

    return quoted + "\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResults::GetSortKey
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionResults::GetSortKey (const std::vector<CollectionDisk> & disks, const DuplicateSet & set)
{
    const DuplicateMember  & first = set.members.front();



    return disks[first.disk].path + '\x1F' + first.file;
}
