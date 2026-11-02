#include "Pch.h"

#include "Machines/Apple2/Common/NativeListing.h"
#include "Machines/Apple2/Common/PascalVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::GetPascalKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * NativeListing::GetPascalKindName (Byte kind)
{
    switch (kind)
    {
        case PascalVolume::kKindBad:       return "Badblocks";
        case PascalVolume::kKindCode:      return "Codefile";
        case PascalVolume::kKindText:      return "Textfile";
        case PascalVolume::kKindInfo:      return "Infofile";
        case PascalVolume::kKindData:      return "Datafile";
        case PascalVolume::kKindGraf:      return "Graffile";
        case PascalVolume::kKindFoto:      return "Fotofile";
        case PascalVolume::kKindSecureDir: return "Securedir";
        default:                           return "Untyped";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::FormatPascalDate
//
//  Nine characters, as the Filer prints them, and blanks for a file with no
//  date.
//
////////////////////////////////////////////////////////////////////////////////

std::string NativeListing::FormatPascalDate (const FileEntry & entry)
{
    static constexpr const char *  kMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    static constexpr int           kCentury  = 100;
    tm                             parts     = {};
    __time64_t                     seconds   = (__time64_t) entry.modifiedUnix;
    errno_t                        err       = 0;



    if (!entry.hasModified)
    {
        return std::string (kDateWidth, ' ');
    }

    err = _gmtime64_s (&parts, &seconds);

    if (err != 0 || parts.tm_mon < 0 || parts.tm_mon >= (int) std::size (kMonths))
    {
        return std::string (kDateWidth, ' ');
    }

    return std::format ("{:2}-{}-{:02}", parts.tm_mday, kMonths[parts.tm_mon], parts.tm_year % kCentury);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::FormatPascal
//
//  The Filer's Extended list: the volume, a row per file in directory order
//  with a row for each free run where it falls, and the totals.
//
////////////////////////////////////////////////////////////////////////////////

void NativeListing::FormatPascal (const VolumeListing & listing, std::vector<std::string> & outLines)
{
    constexpr int  kFirstDataBlock = 6;
    int            at              = kFirstDataBlock;
    int            total           = (int) listing.totalUnits;
    int            largest         = 0;



    auto  addUnused = [&] (int next)
    {
        if (next > at)
        {
            outLines.push_back (std::format ("{:<16}{:>4}{:>{}}{:>6}", "< UNUSED >", next - at, "", kDateWidth + 1, at));
            largest = (std::max) (largest, next - at);
        }
    };

    outLines.clear();
    outLines.push_back (listing.volumeName + ":");

    for (const FileEntry & entry : listing.entries)
    {
        addUnused (entry.firstBlock);

        outLines.push_back (std::format ("{:<16}{:>4} {}{:>6}{:>6}  {}",
                                         entry.name,
                                         entry.sizeUnits,
                                         FormatPascalDate (entry),
                                         entry.firstBlock,
                                         entry.lastBlockBytes,
                                         GetPascalKindName (entry.type)));

        at = (std::max) (at, entry.firstBlock + (int) entry.sizeUnits);
    }

    addUnused (total);

    outLines.push_back (std::format ("{}/{} files<listed/in-dir>, {} blocks used, {} unused, {} in largest",
                                     listing.entries.size(),
                                     listing.directoryFiles,
                                     listing.totalUnits - listing.freeUnits,
                                     listing.freeUnits,
                                     largest));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::MakeCpmSortKey
//
//  The name and type each padded to their directory widths, which is the order
//  STAT sorts by. A name's user prefix isn't part of it.
//
////////////////////////////////////////////////////////////////////////////////

std::string NativeListing::MakeCpmSortKey (const std::string & name)
{
    constexpr size_t  kBaseWidth = 8;
    size_t            dot        = name.find ('.');
    std::string       base       = name.substr (0, dot);
    std::string       type       = (dot == std::string::npos) ? std::string() : name.substr (dot + 1);



    base.resize ((std::max) (base.size(), kBaseWidth), ' ');

    return base + type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::GetCpmName
//
//  The name without the user prefix the volume layer adds for any user but 0.
//
////////////////////////////////////////////////////////////////////////////////

std::string NativeListing::GetCpmName (const FileEntry & entry)
{
    size_t  colon = entry.name.find (':');



    return (entry.user != 0 && colon != std::string::npos) ? entry.name.substr (colon + 1) : entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::AppendCpmRows
//
//  A heading and one user's files in name order. System files are in
//  parentheses, as STAT prints them.
//
////////////////////////////////////////////////////////////////////////////////

void NativeListing::AppendCpmRows (const std::vector<const FileEntry *> & files, std::vector<std::string> & inOutLines)
{
    std::vector<const FileEntry *>  sorted = files;



    std::stable_sort (sorted.begin(), sorted.end(), [] (const FileEntry * a, const FileEntry * b)
    {
        return MakeCpmSortKey (GetCpmName (*a)) < MakeCpmSortKey (GetCpmName (*b));
    });

    inOutLines.push_back ("Recs  Bytes  Ext Acc");

    for (const FileEntry * file : sorted)
    {
        std::string  name = "A:" + GetCpmName (*file);

        inOutLines.push_back (std::format ("{:>4}{:>6}k{:>5} {} {}",
                                           file->records,
                                           file->sizeUnits,
                                           file->extents,
                                           file->isReadOnly ? "R/O" : "R/W",
                                           file->isSystem ? "(" + name + ")" : name));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing::FormatCpm
//
//  STAT *.* for user 0, exactly as CP/M prints it with the current user at 0,
//  including its foot. STAT shows no other user at once, so each of those
//  follows as its own block under a USER line, in user order.
//
////////////////////////////////////////////////////////////////////////////////

void NativeListing::FormatCpm (const VolumeListing & listing, std::vector<std::string> & outLines)
{
    std::map<int, std::vector<const FileEntry *>>  users;



    outLines.clear();

    for (const FileEntry & entry : listing.entries)
    {
        users[entry.user].push_back (&entry);
    }

    if (users.contains (0))
    {
        AppendCpmRows (users[0], outLines);
    }
    else
    {
        outLines.push_back ("File Not Found");
    }

    outLines.push_back (std::format ("Bytes Remaining On A: {}k", listing.freeUnits));

    for (const auto & [user, files] : users)
    {
        if (user != 0)
        {
            outLines.push_back (std::string());
            outLines.push_back (std::format ("USER {}", user));

            AppendCpmRows (files, outLines);
        }
    }
}




