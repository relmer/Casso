#include "Pch.h"

#include "CassoExplorer/Model/GroupChoices.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GroupChoices::GetChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<GroupChoices::Choice> GroupChoices::GetChoices (Folder folder)
{
    using Field = RowGrouping::Field;

    static const Choice  s_kName         = { L"Name",          Field::Name,         nullptr };
    static const Choice  s_kDateModified = { L"Date modified", Field::DateModified, nullptr };
    static const Choice  s_kType         = { L"Type",          Field::Type,         nullptr };
    static const Choice  s_kSize         = { L"Size",          Field::Size,         nullptr };
    static const Choice  s_kDateCreated  = { L"Date created",  Field::None,         L"System.DateCreated" };
    static const Choice  s_kAuthors      = { L"Authors",       Field::None,         L"System.Author" };
    static const Choice  s_kTags         = { L"Tags",          Field::None,         L"System.Keywords" };
    static const Choice  s_kTitle        = { L"Title",         Field::None,         L"System.Title" };
    static const Choice  s_kDate         = { L"Date",          Field::None,         L"System.ItemDate" };
    static const Choice  s_kRating       = { L"Rating",        Field::None,         L"System.Rating" };
    static const Choice  s_kDimensions   = { L"Dimensions",    Field::None,         L"System.Image.Dimensions" };
    static const Choice  s_kLength       = { L"Length",        Field::None,         L"System.Media.Duration" };
    static const Choice  s_kComments     = { L"Comments",      Field::None,         L"System.Comment" };



    switch (folder)
    {
        case Folder::Documents:
            return { s_kName, s_kDateModified, s_kType, s_kSize, s_kDateCreated, s_kAuthors, { L"Categories", Field::None, L"System.Category" }, s_kTags, s_kTitle };

        case Folder::OneDrive:
            return { s_kName, { L"Status", Field::None, L"System.StorageProviderUIStatus" }, s_kDateModified, s_kType, s_kSize, s_kDateCreated, s_kAuthors, s_kTags, s_kTitle };

        case Folder::Search:
            return { s_kName, s_kDateModified, s_kType, s_kSize, { L"Folder", Field::Folder, nullptr }, { L"Relevance", Field::Relevance, nullptr },
                     s_kDateCreated, s_kAuthors, s_kTags, s_kTitle };

        case Folder::Results:
            return { s_kName, s_kType, s_kSize, { L"Folder", Field::Folder, nullptr }, { L"Duplicate set", Field::DuplicateSet, nullptr } };

        case Folder::Drives:
            return { s_kName, s_kType, { L"Total Size", Field::None, L"System.Capacity" }, { L"Free Space", Field::None, L"System.FreeSpace" },
                     { L"File System", Field::None, L"System.Volume.FileSystem" }, s_kComments, { L"Network Location", Field::None, L"System.NetworkLocation" },
                     { L"Percent Full", Field::None, L"System.PercentFull" } };

        case Folder::Pictures:
            return { s_kName, s_kDate, s_kType, s_kSize, s_kTags, s_kDateCreated, s_kDateModified, { L"Date taken", Field::None, L"System.Photo.DateTaken" }, s_kDimensions, s_kRating };

        case Folder::Music:
            return { s_kName, { L"#", Field::None, L"System.Music.TrackNumber" }, s_kTitle, { L"Contributing artists", Field::None, L"System.Music.Artist" },
                     { L"Album", Field::None, L"System.Music.AlbumTitle" }, s_kType, s_kSize, s_kDateCreated, s_kDateModified, { L"Album artist", Field::None, L"System.Music.AlbumArtist" },
                     { L"Bit rate", Field::None, L"System.Audio.EncodingBitrate" }, { L"Genre", Field::None, L"System.Music.Genre" }, s_kLength,
                     { L"Protected", Field::None, L"System.DRM.IsProtected" }, s_kRating, { L"Year", Field::None, L"System.Media.Year" } };

        case Folder::Videos:
            return { s_kName, s_kDate, s_kType, s_kSize, s_kLength, s_kDateCreated, s_kDateModified, { L"Media created", Field::None, L"System.Media.DateEncoded" }, s_kDimensions };

        case Folder::RecycleBin:
            return { s_kName, { L"Original Location", Field::OriginalLocation, nullptr }, { L"Date Deleted", Field::DateDeleted, nullptr }, s_kSize, { L"Item type", Field::Type, nullptr },
                     s_kDateModified, s_kDateCreated, { L"Date accessed", Field::None, L"System.DateAccessed" }, { L"Attributes", Field::None, L"System.FileAttributes" },
                     { L"Perceived type", Field::None, L"System.PerceivedType" }, { L"Owner", Field::None, L"System.FileOwner" }, s_kTags, s_kRating, s_kAuthors, s_kTitle, s_kComments,
                     { L"File description", Field::None, L"System.FileDescription" } };

        case Folder::Home:
            return { s_kName, s_kDateModified, s_kType, s_kSize, { L"Account", Field::None, L"System.WebAccountID" }, { L"Activity", Field::None, L"System.ActivityInfo" },
                     s_kDateCreated, s_kAuthors, s_kTags, s_kTitle, { L"Status", Field::None, L"System.StorageProviderUIStatus" } };

        case Folder::Gallery:
            return { s_kName, s_kDateModified, s_kType, s_kSize, { L"Folder", Field::Folder, nullptr }, { L"Relevance", Field::Relevance, nullptr }, s_kDateCreated, s_kAuthors, s_kTags, s_kTitle,
                     s_kDimensions, { L"Width", Field::None, L"System.Image.HorizontalSize" }, { L"Height", Field::None, L"System.Image.VerticalSize" }, s_kDate,
                     { L"Kind", Field::None, L"System.Kind" }, s_kLength, { L"Frame height", Field::None, L"System.Video.FrameHeight" }, { L"Frame width", Field::None, L"System.Video.FrameWidth" } };

        case Folder::Network:
            return { s_kName, { L"Category", Field::None, L"System.Category" }, { L"Workgroup", Field::None, L"System.Computer.Workgroup" }, { L"Network location", Field::None, L"System.NetworkLocation" },
                     { L"Discovery Method", Field::None, L"System.NetworkProvider" }, { L"MAC Address", Field::None, L"{656A3BB3-ECC0-43FD-8477-4AE0404A96CD} 12294" }, { L"IP Address", Field::None, L"System.Devices.IpAddress" } };

        case Folder::Zip:
            return { s_kName, s_kType, { L"Compressed size", Field::None, L"System.ZipFolder.CompressedSize" }, { L"Password protected", Field::None, L"System.ZipFolder.Encrypted" },
                     { L"Method", Field::None, L"System.ZipFolder.Method" }, s_kSize, { L"Ratio", Field::None, L"System.ZipFolder.Ratio" }, s_kDateModified, { L"CRC-32", Field::None, L"System.ZipFolder.CRC32" } };

        default:
            return { s_kName, s_kDateModified, s_kType, s_kSize, s_kDateCreated, s_kAuthors, s_kTags, s_kTitle };
    }
}
