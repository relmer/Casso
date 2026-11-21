#include "Pch.h"

#include "CassoExplorer/Model/HostFolderReader.h"
#include "Core/PathResolver.h"
#include "Devices/Disk/BlankDiskSpecResolver.h"
#include "Devices/Disk/Containers/ImageContainer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReader::Classify
//
//  The entries the folder options let through, each with its kind and label. A
//  folder is labeled by the name in its desktop.ini, which is read only for a
//  read-only or system folder, so no other folder costs a read, and never on
//  the network, which could wait on a share that has stopped answering.
//
////////////////////////////////////////////////////////////////////////////////

void HostFolderReader::Classify (const std::wstring & folder, const std::vector<FileSystemEntry> & entries, std::vector<HostFolderItem> & outItems) const
{
    bool  network = PathResolver::IsNetworkPath (folder);



    outItems.clear();

    for (const FileSystemEntry & entry : entries)
    {
        HostFolderItem  item;

        if (!IsShown (entry))
        {
            continue;
        }

        item.entry = entry;
        item.path  = JoinPath (folder, entry.name);
        item.label = entry.name;

        if (entry.isFolder)
        {
            item.kind = HostFolderItem::Kind::Folder;

            if ((entry.isReadOnly || entry.isSystem) && !network && m_localizedName)
            {
                std::wstring  localized = m_localizedName (item.path);

                item.label = localized.empty() ? item.label : localized;
            }
        }
        else if (IsSupportedImage (entry.name))
        {
            item.kind = HostFolderItem::Kind::Image;
        }
        else if (IsArchive (item.path))
        {
            item.kind = HostFolderItem::Kind::Archive;
        }

        outItems.push_back (std::move (item));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReader::IsShellImage
//
//  Whether an item the shell lists is a disk image: a file with an image's
//  extension, by its path on the disk, or for one inside a zip, which has
//  no path, by its name.
//
////////////////////////////////////////////////////////////////////////////////

bool HostFolderReader::IsShellImage (const IShellItemVerbs::ShellFolderItem & item)
{
    return (item.isFile || !item.isFolder) && IsSupportedImage (item.path.empty() ? item.name : item.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReader::JoinPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFolderReader::JoinPath (const std::wstring & folder, const std::wstring & name)
{
    if (folder.empty() || folder.back() == L'\\' || folder.back() == L'/')
    {
        return folder + name;
    }

    return folder + L"\\" + name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReader::IsSupportedImage
//
//  The container words the disk tool advertises, as extensions.
//
////////////////////////////////////////////////////////////////////////////////

bool HostFolderReader::IsSupportedImage (const std::wstring & fileName)
{
    size_t                                        count      = 0;
    const BlankDiskSpecResolver::ContainerName  * containers = BlankDiskSpecResolver::GetAdvertisedContainers (count);
    size_t                                        dot        = fileName.rfind (L'.');
    size_t                                        i          = 0;
    std::string                                   extension;



    if (dot == std::wstring::npos)
    {
        return false;
    }

    for (wchar_t c : fileName.substr (dot + 1))
    {
        extension += (char) ((c >= L'A' && c <= L'Z') ? (c - L'A' + L'a') : (c & 0x7F));
    }

    for (i = 0; i < count; i++)
    {
        if (extension == containers[i].name)
        {
            return true;
        }
    }

    return ImageContainer::IsListedExtension (extension);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReader::IsArchive
//
////////////////////////////////////////////////////////////////////////////////

bool HostFolderReader::IsArchive (const std::wstring & path) const
{
    std::wstring  extension = std::filesystem::path (path).extension().wstring();
    auto          known     = m_archiveExtensions.end();



    if (m_shellVerbs == nullptr || extension.empty() || IsSupportedImage (path))
    {
        return false;
    }

    std::transform (extension.begin(), extension.end(), extension.begin(), ::towlower);

    known = m_archiveExtensions.find (extension);

    if (known == m_archiveExtensions.end())
    {
        known = m_archiveExtensions.emplace (extension, m_shellVerbs->IsArchive (path)).first;
    }

    return known->second;
}
