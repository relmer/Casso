#include "Pch.h"

#include "FileTypes/ZMachineStory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The ZSCII characters 155 to 223 of the default Unicode translation
//  table, from section 3.8.5.3 of the standard.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t  s_kDefaultUnicode[] =
{
    0x00E4, 0x00F6, 0x00FC, 0x00C4, 0x00D6, 0x00DC, 0x00DF, 0x00BB, 0x00AB, 0x00EB, 0x00EF, 0x00FF, 0x00CB, 0x00CF,
    0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA, 0x00FD, 0x00C1, 0x00C9, 0x00CD, 0x00D3, 0x00DA, 0x00DD,
    0x00E0, 0x00E8, 0x00EC, 0x00F2, 0x00F9, 0x00C0, 0x00C8, 0x00CC, 0x00D2, 0x00D9,
    0x00E2, 0x00EA, 0x00EE, 0x00F4, 0x00FB, 0x00C2, 0x00CA, 0x00CE, 0x00D4, 0x00DB,
    0x00E5, 0x00C5, 0x00F8, 0x00D8, 0x00E3, 0x00F1, 0x00F5, 0x00C3, 0x00D1, 0x00D5,
    0x00E6, 0x00C6, 0x00E7, 0x00C7, 0x00FE, 0x00F0, 0x00DE, 0x00D0, 0x00A3, 0x0153, 0x0152, 0x00A1, 0x00BF,
};

static constexpr int  s_kFirstExtraZscii = 155;





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ReadHeader
//
//  Section 11 of the standard. The length is in units of 2, 4 or 8 bytes
//  by version, and a version 1 or 2 story may have none.
//
////////////////////////////////////////////////////////////////////////////////

bool ZMachineStory::ReadHeader (std::span<const Byte> bytes, Header & outHeader)
{
    auto  word = [&bytes] (size_t at) { return (Word) ((bytes[at] << 8) | bytes[at + 1]); };
    int   scale = 0;



    outHeader = Header();

    if (bytes.size() < kHeaderBytes || bytes[0] < 1 || bytes[0] > 8)
    {
        return false;
    }

    outHeader.version        = bytes[0];
    outHeader.flags1         = bytes[1];
    outHeader.release        = word (0x02);
    outHeader.highMemory     = word (0x04);
    outHeader.initialPc      = word (0x06);
    outHeader.dictionary     = word (0x08);
    outHeader.objects        = word (0x0A);
    outHeader.globals        = word (0x0C);
    outHeader.staticMemory   = word (0x0E);
    outHeader.flags2         = word (0x10);
    outHeader.abbreviations  = word (0x18);
    outHeader.checksum       = word (0x1C);
    outHeader.routinesOffset = word (0x28);
    outHeader.stringsOffset  = word (0x2A);
    outHeader.alphabet       = (outHeader.version >= 5) ? word (0x34) : (Word) 0;
    outHeader.extension      = (outHeader.version >= 5) ? word (0x36) : (Word) 0;

    for (size_t at = 0; at < 6; at++)
    {
        outHeader.serial[at] = (char) bytes[0x12 + at];
    }

    scale             = (outHeader.version <= 3) ? 2 : (outHeader.version <= 5) ? 4 : 8;
    outHeader.length  = (uint32_t) word (0x1A) * (uint32_t) scale;

    //  Every table is inside the file, and the header is in dynamic memory.
    return outHeader.staticMemory >= kHeaderBytes && outHeader.staticMemory <= bytes.size()
        && outHeader.highMemory <= bytes.size() && outHeader.dictionary < bytes.size()
        && outHeader.objects >= kHeaderBytes && outHeader.objects < outHeader.staticMemory
        && outHeader.globals >= kHeaderBytes && outHeader.globals < outHeader.staticMemory
        && outHeader.abbreviations < bytes.size()
        && outHeader.length <= bytes.size() + (size_t) scale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ComputeChecksum
//
//  The bytes from $40 to the end of the file by the header's length, added up
//  modulo $10000, as verify does.
//
////////////////////////////////////////////////////////////////////////////////

Word ZMachineStory::ComputeChecksum (std::span<const Byte> bytes, uint32_t length)
{
    uint32_t  sum = 0;
    size_t    end = std::min ((size_t) length, bytes.size());



    for (size_t at = kHeaderBytes; at < end; at++)
    {
        sum += bytes[at];
    }

    return (Word) sum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::IsVerifiedStory
//
////////////////////////////////////////////////////////////////////////////////

bool ZMachineStory::IsVerifiedStory (std::span<const Byte> bytes)
{
    Header  header;



    return ReadHeader (bytes, header) && header.length > kHeaderBytes && header.length <= bytes.size()
        && header.checksum == ComputeChecksum (bytes, header.length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool ZMachineStory::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    Header  header;
    bool    named = false;



    for (const char * suffix : { ".z1", ".z2", ".z3", ".z4", ".z5", ".z6", ".z7", ".z8", ".Z1", ".Z2", ".Z3", ".Z4", ".Z5", ".Z6", ".Z7", ".Z8" })
    {
        named = named || facts.IsNameEnding (suffix);
    }

    return IsVerifiedStory (bytes) || (named && ReadHeader (bytes, header));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZMachineStory::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    Story              story;
    std::vector<Word>  propertyWords;
    size_t             count = 0;
    bool               read  = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    outFile        = DecodedFile();
    outFile.format = FileFormat::InfocomStory;
    outFile.kind   = DecodedFile::Kind::Lines;

    story.bytes = bytes;
    read        = ReadHeader (bytes, story.header);

    if (!read)
    {
        outError = L"This isn't a Z-machine story file.";
        return HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    }

    if (story.header.length > kHeaderBytes && story.header.length < bytes.size())
    {
        story.bytes = bytes.first (story.header.length);
    }

    LoadAlphabets (story);

    ListHeader (story, outFile.lines);
    ListAbbreviations (story, outFile.lines);
    ListDictionary (story, outFile.lines);
    count = ListObjects (story, outFile.lines, propertyWords);
    ListTree (story, count, outFile.lines);
    ListStrings (story, propertyWords, outFile.lines);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::LoadAlphabets
//
//  Section 3.5: the three alphabets, the third's first two places the
//  escape and, from version 2, a new line. A version 5 story may have other
//  alphabets, and Unicode for ZSCII 155 to 251, in tables the header
//  points to.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::LoadAlphabets (Story & story)
{
    Word    table = 0;
    size_t  count = 0;



    for (int at = 0; at < 26; at++)
    {
        story.alphabets[0][at] = (wchar_t) (L'a' + at);
        story.alphabets[1][at] = (wchar_t) (L'A' + at);
        story.alphabets[2][at] = (story.header.version == 1) ? s_kA2v1[at] : s_kA2[at];
    }

    story.unicode.assign (std::begin (s_kDefaultUnicode), std::end (s_kDefaultUnicode));

    //  The Unicode table is the third entry of the header extension table.
    if (story.header.extension != 0 && (size_t) story.header.extension + 8 <= story.bytes.size() && GetWord (story, story.header.extension) >= 3)
    {
        table = GetWord (story, story.header.extension + 6);

        if (table != 0 && table < story.bytes.size())
        {
            count = GetByte (story, table);

            story.unicode.clear();

            for (size_t at = 0; at < count; at++)
            {
                story.unicode.push_back ((wchar_t) GetWord (story, table + 1 + at * 2));
            }
        }
    }

    if (story.header.alphabet != 0 && (size_t) story.header.alphabet + 78 <= story.bytes.size())
    {
        for (int set = 0; set < 3; set++)
        {
            for (int at = (set == 2) ? 2 : 0; at < 26; at++)
            {
                std::wstring  one;

                AppendZscii (story, GetByte (story, story.header.alphabet + set * 26 + at), one);
                story.alphabets[set][at] = one.empty() ? L'?' : one[0];
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ReadString
//
//  Section 3: three Z-characters a word until the word with its top bit
//  set. Versions 1 and 2 shift alphabets for one character or until the
//  next lock; later versions shift for one. An abbreviation's string has no
//  abbreviations in it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ZMachineStory::ReadString (const Story & story, size_t address, size_t & outEnd, bool abbreviations)
{
    std::vector<int>  zchars;
    std::wstring      text;
    size_t            at       = address;
    int               version  = story.header.version;
    int               locked   = 0;
    int               shifted  = -1;
    int               escape   = 0;
    int               high     = 0;
    int               abbrev   = -1;
    bool              skip     = false;



    for (;;)
    {
        Word  word = 0;

        if (at + 1 >= story.bytes.size() || zchars.size() > s_kMaxStringChars)
        {
            break;
        }

        word = GetWord (story, at);
        at  += 2;

        zchars.push_back ((word >> 10) & 0x1F);
        zchars.push_back ((word >> 5) & 0x1F);
        zchars.push_back (word & 0x1F);

        if (word & 0x8000)
        {
            break;
        }
    }

    outEnd = at;

    for (int zc : zchars)
    {
        int  set = (shifted >= 0) ? shifted : locked;

        if (skip)
        {
            skip = false;
            continue;
        }

        if (abbrev >= 0)
        {
            size_t  ignored = 0;
            Word    entry   = GetWord (story, story.header.abbreviations + (size_t) (32 * abbrev + zc) * 2);

            text  += ReadString (story, (size_t) entry * 2, ignored, false);
            abbrev = -1;
            continue;
        }

        if (escape == 1)
        {
            high   = zc;
            escape = 2;
            continue;
        }

        if (escape == 2)
        {
            AppendZscii (story, (high << 5) | zc, text);
            escape = 0;
            continue;
        }

        if (zc == 0)
        {
            text   += L' ';
            shifted = -1;
            continue;
        }

        if (zc < 6)
        {
            if (version >= 3 && zc <= 3)
            {
                abbrev = zc - 1;
            }
            else if (version >= 3)
            {
                shifted = zc - 3;
            }
            else if (zc == 1)
            {
                if (version == 1)
                {
                    text += L'\n';
                }
                else
                {
                    abbrev = 0;
                }
            }
            else if (zc <= 3)
            {
                shifted = (locked + zc - 1) % 3;
            }
            else
            {
                locked = (locked + zc - 3) % 3;
            }

            //  An abbreviation inside an abbreviation is skipped, index and all.
            if (abbrev >= 0 && !abbreviations)
            {
                abbrev = -1;
                skip   = true;
            }

            continue;
        }

        if (set == 2 && zc == 6)
        {
            escape = 1;
        }
        else
        {
            text += story.alphabets[set][zc - 6];
        }

        shifted = -1;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::AppendZscii
//
//  Section 3.8: ASCII from 32 to 126, a new line for 13 and the Unicode
//  table's character from 155.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::AppendZscii (const Story & story, int zscii, std::wstring & ioText)
{
    if (zscii == 13)
    {
        ioText += L'\n';
    }
    else if (zscii == 9 || zscii == 11)
    {
        ioText += L' ';
    }
    else if (zscii >= 32 && zscii <= 126)
    {
        ioText += (wchar_t) zscii;
    }
    else if (zscii >= s_kFirstExtraZscii && (size_t) (zscii - s_kFirstExtraZscii) < story.unicode.size())
    {
        ioText += story.unicode[(size_t) (zscii - s_kFirstExtraZscii)];
    }
    else if (zscii != 0)
    {
        ioText += L'?';
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::UnpackRoutine
//
//  Section 1.2.3.
//
////////////////////////////////////////////////////////////////////////////////

size_t ZMachineStory::UnpackRoutine (const Story & story, Word packed)
{
    switch (story.header.version)
    {
        case 1: case 2: case 3: return (size_t) packed * 2;
        case 4: case 5:         return (size_t) packed * 4;
        case 6: case 7:         return (size_t) packed * 4 + (size_t) story.header.routinesOffset * 8;
        default:                return (size_t) packed * 8;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::UnpackString
//
////////////////////////////////////////////////////////////////////////////////

size_t ZMachineStory::UnpackString (const Story & story, Word packed)
{
    switch (story.header.version)
    {
        case 1: case 2: case 3: return (size_t) packed * 2;
        case 4: case 5:         return (size_t) packed * 4;
        case 6: case 7:         return (size_t) packed * 4 + (size_t) story.header.stringsOffset * 8;
        default:                return (size_t) packed * 8;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::GetWord
//
//  Zero past the end, so a damaged table reads as empty.
//
////////////////////////////////////////////////////////////////////////////////

Word ZMachineStory::GetWord (const Story & story, size_t address)
{
    return (address + 1 < story.bytes.size()) ? (Word) ((story.bytes[address] << 8) | story.bytes[address + 1]) : (Word) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::GetByte
//
////////////////////////////////////////////////////////////////////////////////

Byte ZMachineStory::GetByte (const Story & story, size_t address)
{
    return (address < story.bytes.size()) ? story.bytes[address] : (Byte) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ListHeader
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::ListHeader (const Story & story, std::vector<std::wstring> & ioLines)
{
    const Header  & h      = story.header;
    std::wstring    serial;
    Word            actual = ComputeChecksum (story.bytes, h.length > 0 ? h.length : (uint32_t) story.bytes.size());



    for (char c : h.serial)
    {
        if (c >= ' ' && c <= '~')
        {
            serial += (wchar_t) c;
        }
    }

    ioLines.push_back (std::format (L"Z-machine story, version {}", h.version));
    ioLines.push_back (std::format (L"Release {}, serial number {}", h.release, serial));
    ioLines.push_back (std::format (L"Length {} bytes", h.length > 0 ? h.length : (uint32_t) story.bytes.size()));
    ioLines.push_back (h.checksum == actual ? std::format (L"Checksum ${:04X}, which matches", h.checksum)
                                            : std::format (L"Checksum ${:04X}; the file's is ${:04X}", h.checksum, actual));
    ioLines.push_back (std::format (L"Flags ${:02X} ${:04X}", h.flags1, h.flags2));
    ioLines.push_back (std::format (L"High memory ${:04X}", h.highMemory));
    ioLines.push_back (std::format (L"Static memory ${:04X}", h.staticMemory));
    ioLines.push_back ((h.version == 6 || h.version == 7) ? std::format (L"Main routine ${:04X}", h.initialPc) : std::format (L"Initial PC ${:04X}", h.initialPc));
    ioLines.push_back (std::format (L"Dictionary ${:04X}", h.dictionary));
    ioLines.push_back (std::format (L"Object table ${:04X}", h.objects));
    ioLines.push_back (std::format (L"Global variables ${:04X}", h.globals));
    ioLines.push_back (std::format (L"Abbreviations ${:04X}", h.abbreviations));

    if (h.version == 6 || h.version == 7)
    {
        ioLines.push_back (std::format (L"Routines offset ${:04X}, strings offset ${:04X}", h.routinesOffset, h.stringsOffset));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ListAbbreviations
//
//  None in version 1, 32 in version 2, and 96 in later versions.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::ListAbbreviations (const Story & story, std::vector<std::wstring> & ioLines)
{
    size_t  count = (story.header.version == 1) ? 0 : (story.header.version == 2) ? 32 : 96;



    if (count == 0 || story.header.abbreviations == 0)
    {
        return;
    }

    ioLines.push_back (std::wstring());
    ioLines.push_back (L"Abbreviations");

    for (size_t at = 0; at < count; at++)
    {
        size_t  end     = 0;
        Word    address = GetWord (story, story.header.abbreviations + at * 2);

        ioLines.push_back (std::format (L"  {:>2}  {}", at, Quote (ReadString (story, (size_t) address * 2, end, false))));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ListDictionary
//
//  Section 13: the word separators, the length of an entry and the count,
//  then the entries, whose text is 4 bytes before version 4 and 6 after.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::ListDictionary (const Story & story, std::vector<std::wstring> & ioLines)
{
    size_t        at          = story.header.dictionary;
    size_t        separators  = GetByte (story, at);
    std::wstring  marks;
    size_t        entryLength = 0;
    int           count       = 0;



    for (size_t s = 0; s < separators; s++)
    {
        AppendZscii (story, GetByte (story, at + 1 + s), marks);
        marks += L' ';
    }

    at         += 1 + separators;
    entryLength = GetByte (story, at);
    count       = (int16_t) GetWord (story, at + 1);
    at         += 3;
    count       = std::abs (count);

    ioLines.push_back (std::wstring());
    ioLines.push_back (std::format (L"Dictionary: {} words, separators {}", count, marks));

    for (int word = 0; word < count && entryLength > 0 && at + entryLength <= story.bytes.size(); word++, at += entryLength)
    {
        size_t  end = 0;

        ioLines.push_back (std::format (L"  ${:04X}  {}", at, ReadString (story, at, end, false)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::GetObjectAddress
//
//  Section 12: the property defaults, 31 words before version 4 and 63
//  after, then entries of 9 bytes, or 14 from version 4.
//
////////////////////////////////////////////////////////////////////////////////

size_t ZMachineStory::GetObjectAddress (const Story & story, size_t number)
{
    bool  isEarly = story.header.version <= 3;



    return (size_t) story.header.objects + (isEarly ? 31 : 63) * 2 + (number - 1) * (isEarly ? 9 : 14);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::GetPropertyTable
//
////////////////////////////////////////////////////////////////////////////////

size_t ZMachineStory::GetPropertyTable (const Story & story, size_t number)
{
    return GetWord (story, GetObjectAddress (story, number) + (story.header.version <= 3 ? 7 : 12));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::GetObjectLink
//
////////////////////////////////////////////////////////////////////////////////

int ZMachineStory::GetObjectLink (const Story & story, size_t number, int which)
{
    size_t  entry = GetObjectAddress (story, number);



    return (story.header.version <= 3) ? GetByte (story, entry + 4 + (size_t) which) : GetWord (story, entry + 6 + (size_t) which * 2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::GetObjectName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ZMachineStory::GetObjectName (const Story & story, size_t number)
{
    size_t  table = GetPropertyTable (story, number);
    size_t  end   = 0;



    return (GetByte (story, table) == 0) ? std::wstring() : ReadString (story, table + 1, end, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ListObjects
//
//  The objects are numbered from 1; the first property table, which comes
//  after the last entry, is where the entries end. The values of
//  two-byte properties are returned, since a routine's address is one.
//
////////////////////////////////////////////////////////////////////////////////

size_t ZMachineStory::ListObjects (const Story & story, std::vector<std::wstring> & ioLines, std::vector<Word> & outPropertyWords)
{
    bool    isEarly = story.header.version <= 3;
    size_t  lowest  = story.bytes.size();
    size_t  count   = 0;
    size_t  maximum = isEarly ? 255 : s_kMaxObjects;



    outPropertyWords.clear();

    while (count < maximum && GetObjectAddress (story, count + 1) + (isEarly ? 9 : 14) <= lowest)
    {
        size_t  table = GetPropertyTable (story, count + 1);

        if (table < kHeaderBytes || table >= story.bytes.size())
        {
            break;
        }

        lowest = std::min (lowest, table);
        count++;
    }

    ioLines.push_back (std::wstring());
    ioLines.push_back (std::format (L"Objects: {}", count));

    for (size_t number = 1; number <= count; number++)
    {
        size_t        entry      = GetObjectAddress (story, number);
        size_t        table      = GetPropertyTable (story, number);
        size_t        at         = table + 1 + (size_t) GetByte (story, table) * 2;
        std::wstring  attributes;
        std::wstring  properties;

        for (int bit = 0; bit < (isEarly ? 32 : 48); bit++)
        {
            if (GetByte (story, entry + (size_t) bit / 8) & (0x80 >> (bit % 8)))
            {
                attributes += std::format (L" {}", bit);
            }
        }

        for (Byte size = GetByte (story, at); size != 0 && at < story.bytes.size(); size = GetByte (story, at))
        {
            size_t  number2 = isEarly ? (size & 0x1F) : (size & 0x3F);
            size_t  length  = isEarly ? (size_t) (size >> 5) + 1 : (size & 0x80) ? (GetByte (story, at + 1) & 0x3F) : (size & 0x40) ? 2 : 1;
            size_t  data    = (!isEarly && (size & 0x80)) ? at + 2 : at + 1;

            if (!isEarly && (size & 0x80) && length == 0)
            {
                length = 64;
            }

            properties += std::format (L" {}:", number2);

            for (size_t b = 0; b < length; b++)
            {
                properties += std::format (L"{}{:02X}", b == 0 ? L"" : L" ", GetByte (story, data + b));
            }

            if (length == 2)
            {
                outPropertyWords.push_back (GetWord (story, data));
            }

            at = data + length;
        }

        ioLines.push_back (std::format (L"  {:>3}  {}", number, Quote (GetObjectName (story, number))));
        ioLines.push_back (std::format (L"       parent {}, sibling {}, child {}", GetObjectLink (story, number, 0), GetObjectLink (story, number, 1), GetObjectLink (story, number, 2)));
        ioLines.push_back (L"       attributes" + (attributes.empty() ? std::wstring (L" none") : attributes));
        ioLines.push_back (L"       properties" + (properties.empty() ? std::wstring (L" none") : properties));
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ListTree
//
//  The objects with no parent, each with its children below it.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::ListTree (const Story & story, size_t count, std::vector<std::wstring> & ioLines)
{
    std::vector<std::pair<size_t, int>>  stack;
    std::set<size_t>                     listed;



    if (count == 0)
    {
        return;
    }

    ioLines.push_back (std::wstring());
    ioLines.push_back (L"Object tree");

    for (size_t root = count; root >= 1; root--)
    {
        if (GetObjectLink (story, root, 0) == 0)
        {
            stack.push_back ({ root, 1 });
        }
    }

    while (!stack.empty())
    {
        auto [number, depth] = stack.back();
        std::vector<size_t>  children;

        stack.pop_back();

        if (number == 0 || number > count || !listed.insert (number).second)
        {
            continue;
        }

        ioLines.push_back (std::wstring ((size_t) depth * 2, L' ') + std::format (L"[{}] {}", number, GetObjectName (story, number)));

        for (int child = GetObjectLink (story, number, 2); child != 0 && (size_t) child <= count && children.size() <= count; child = GetObjectLink (story, (size_t) child, 1))
        {
            children.push_back ((size_t) child);
        }

        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            stack.push_back ({ *it, depth + 1 });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::ListStrings
//
//  The text each routine found prints, then the strings after the last
//  routine until one doesn't decode as text.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::ListStrings (const Story & story, const std::vector<Word> & propertyWords, std::vector<std::wstring> & ioLines)
{
    Found                found;
    std::vector<size_t>  pending;
    size_t               unit    = (story.header.version <= 3) ? 2 : (story.header.version <= 7) ? 4 : 8;
    size_t               at      = 0;



    //  Before version 6 the first instruction has no routine header before it.
    if (story.header.version == 6 || story.header.version == 7)
    {
        pending.push_back (UnpackRoutine (story, story.header.initialPc));
    }
    else
    {
        FollowRoutine (story, (size_t) story.header.initialPc | ((size_t) 1 << 31), found, pending);
    }

    for (Word value : propertyWords)
    {
        pending.push_back (UnpackRoutine (story, value));
    }

    for (size_t global = 0; global < 240; global++)
    {
        pending.push_back (UnpackRoutine (story, GetWord (story, story.header.globals + global * 2)));
    }

    while (!pending.empty() && found.routines.size() < s_kMaxRoutines)
    {
        size_t  routine = pending.back();

        pending.pop_back();

        if (found.routines.count (routine) == 0 && IsRoutine (story, routine))
        {
            FollowRoutine (story, routine, found, pending);
        }
    }

    //  The strings after the code, in order.
    at = (found.codeEnd + unit - 1) / unit * unit;

    while (at < story.bytes.size() && found.codeEnd > 0)
    {
        size_t        end  = 0;
        std::wstring  text = ReadString (story, at, end, true);

        if (text.find_first_not_of (L" \n") == std::wstring::npos || text.find (L'?') != std::wstring::npos)
        {
            break;
        }

        found.strings.emplace (at, text);
        at = (end + unit - 1) / unit * unit;
    }

    ioLines.push_back (std::wstring());
    ioLines.push_back (std::format (L"Strings: {}", found.strings.size()));

    for (const auto & [address, text] : found.strings)
    {
        ioLines.push_back (std::format (L"  ${:05X}  {}", address, Quote (text)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::IsRoutine
//
//  An address in high memory with a count of locals from 0 to 15, whose
//  first instruction decodes.
//
////////////////////////////////////////////////////////////////////////////////

bool ZMachineStory::IsRoutine (const Story & story, size_t address)
{
    Instruction  first;
    Byte         locals = GetByte (story, address);
    size_t       code   = address + 1 + ((story.header.version <= 4) ? (size_t) locals * 2 : 0);



    return address >= story.header.highMemory && address > kHeaderBytes && address < story.bytes.size() && locals <= 15
        && code < story.bytes.size() && DecodeInstruction (story, code, first);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::FollowRoutine
//
//  Each instruction reachable in the routine, by falling through, by
//  branch and by jump. A routine called with a constant address is added
//  to the ones pending. The top bit of `routine` marks a first instruction
//  with no routine header before it.
//
////////////////////////////////////////////////////////////////////////////////

void ZMachineStory::FollowRoutine (const Story & story, size_t routine, Found & ioFound, std::vector<size_t> & ioPending)
{
    std::vector<size_t>  work;
    std::set<size_t>     seen;
    bool                 bare   = (routine & ((size_t) 1 << 31)) != 0;
    size_t               start  = routine & ~((size_t) 1 << 31);
    Byte                 locals = bare ? (Byte) 0 : GetByte (story, start);



    ioFound.routines.insert (start);

    work.push_back (bare ? start : start + 1 + ((story.header.version <= 4) ? (size_t) locals * 2 : 0));

    while (!work.empty())
    {
        Instruction  instruction;
        size_t       at   = work.back();
        size_t       next = 0;

        work.pop_back();

        if (!seen.insert (at).second || !DecodeInstruction (story, at, instruction))
        {
            continue;
        }

        next            = at + instruction.length;
        ioFound.codeEnd = std::max (ioFound.codeEnd, next);

        if (instruction.hasText)
        {
            ioFound.strings.emplace (at, instruction.text);
        }

        if (instruction.printsPaddr && !instruction.operands.empty() && !instruction.operands[0].isVariable)
        {
            size_t        end     = 0;
            size_t        address = UnpackString (story, instruction.operands[0].value);

            if (address < story.bytes.size())
            {
                ioFound.strings.emplace (address, ReadString (story, address, end, true));
            }
        }

        if (instruction.isCall && !instruction.operands.empty() && !instruction.operands[0].isVariable && instruction.operands[0].value != 0)
        {
            ioPending.push_back (UnpackRoutine (story, instruction.operands[0].value));
        }

        if (instruction.branches && instruction.branchTo != 0 && instruction.branchTo != 1)
        {
            work.push_back ((size_t) ((int64_t) next + instruction.branchTo - 2));
        }

        if (instruction.isJump && !instruction.operands.empty() && !instruction.operands[0].isVariable)
        {
            work.push_back ((size_t) ((int64_t) next + (int16_t) instruction.operands[0].value - 2));
        }

        if (!instruction.ends)
        {
            work.push_back (next);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::DecodeInstruction
//
//  Section 4: the long, short, variable and extended forms, their operand
//  types, then the store variable, the branch and the inline text the
//  opcode has.
//
////////////////////////////////////////////////////////////////////////////////

bool ZMachineStory::DecodeInstruction (const Story & story, size_t address, Instruction & outInstruction)
{
    Byte                 first    = GetByte (story, address);
    size_t               at       = address + 1;
    int                  count    = 0;          // 0, 1 or 2 for 0OP, 1OP and 2OP; 3 for VAR
    int                  number   = 0;
    bool                 extended = false;
    std::vector<int>     types;



    outInstruction         = Instruction();
    outInstruction.address = address;

    if (address >= story.bytes.size())
    {
        return false;
    }

    if (first == 0xBE && story.header.version >= 5)
    {
        extended = true;
        count    = 3;
        number   = GetByte (story, at++);
    }
    else if ((first & 0xC0) == 0xC0)
    {
        count  = (first & 0x20) ? 3 : 2;
        number = first & 0x1F;
    }
    else if ((first & 0xC0) == 0x80)
    {
        int  type = (first >> 4) & 3;

        count  = (type == 3) ? 0 : 1;
        number = first & 0x0F;

        if (type != 3)
        {
            types.push_back (type);
        }
    }
    else
    {
        count  = 2;
        number = first & 0x1F;
        types.push_back ((first & 0x40) ? 2 : 1);
        types.push_back ((first & 0x20) ? 2 : 1);
    }

    //  The variable and extended forms have their types in a byte, or two
    //  for call_vs2 and call_vn2.
    if (extended || (first & 0xC0) == 0xC0)
    {
        int  typeBytes = (!extended && count == 3 && (number == 12 || number == 26)) ? 2 : 1;

        for (int b = 0; b < typeBytes; b++)
        {
            Byte  byte = GetByte (story, at++);

            for (int shift = 6; shift >= 0; shift -= 2)
            {
                types.push_back ((byte >> shift) & 3);
            }
        }

        while (!types.empty() && types.back() == 3)
        {
            types.pop_back();
        }

        if (std::find (types.begin(), types.end(), 3) != types.end())
        {
            return false;
        }
    }

    for (int type : types)
    {
        Operand  operand;

        operand.isVariable = type == 2;
        operand.value      = (type == 0) ? GetWord (story, at) : GetByte (story, at);
        at                += (type == 0) ? 2 : 1;

        outInstruction.operands.push_back (operand);
    }

    if (!NameOpcode (story, count, number, extended, outInstruction))
    {
        return false;
    }

    if (outInstruction.stores)
    {
        at++;
    }

    if (outInstruction.branches)
    {
        Byte  branch = GetByte (story, at++);

        outInstruction.branchOn = (branch & 0x80) != 0;

        if (branch & 0x40)
        {
            outInstruction.branchTo = branch & 0x3F;
        }
        else
        {
            int  offset = ((branch & 0x3F) << 8) | GetByte (story, at++);

            outInstruction.branchTo = (offset & 0x2000) ? offset - 0x4000 : offset;
        }
    }

    if (outInstruction.hasText)
    {
        size_t  end = 0;

        outInstruction.text = ReadString (story, at, end, true);
        at                  = end;
    }

    outInstruction.length = at - address;

    return at <= story.bytes.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::NameOpcode
//
//  Section 14's table: whether each opcode stores, branches, has text,
//  ends the flow, jumps or calls, by version. False for one with no
//  meaning in the story's version.
//
////////////////////////////////////////////////////////////////////////////////

bool ZMachineStory::NameOpcode (const Story & story, int count, int number, bool extended, Instruction & ioInstruction)
{
    int    v  = story.header.version;
    auto & op = ioInstruction;



    if (extended)
    {
        if (number > 29 || number == 14 || number == 15)
        {
            return false;
        }

        op.stores   = number <= 4 || number == 9 || number == 10 || number == 12 || number == 19 || number == 29;
        op.branches = number == 6 || number == 24 || number == 27;
        return true;
    }

    switch (count)
    {
        case 0:
            switch (number)
            {
                case 0: case 1: case 7: case 8: case 10:
                    op.ends = true;
                    return true;

                case 2:
                    op.hasText = true;
                    return true;

                case 3:
                    op.hasText = true;
                    op.ends    = true;
                    return true;

                case 5: case 6:
                    op.branches = v <= 3;
                    op.stores   = v == 4;
                    return v <= 4;

                case 9:
                    op.stores = v >= 5;
                    return true;

                case 13: case 15:
                    op.branches = true;
                    return true;

                case 14:
                    return false;

                default:
                    return true;
            }

        case 1:
            switch (number)
            {
                case 0:
                    op.branches = true;
                    return true;

                case 1: case 2:
                    op.stores   = true;
                    op.branches = true;
                    return true;

                case 3: case 4: case 14:
                    op.stores = true;
                    return true;

                case 8:
                    op.stores = true;
                    op.isCall = true;
                    return v >= 4;

                case 11:
                    op.ends = true;
                    return true;

                case 12:
                    op.ends   = true;
                    op.isJump = true;
                    return true;

                case 13:
                    op.printsPaddr = true;
                    return true;

                case 15:
                    op.stores = v <= 4;
                    op.isCall = v >= 5;
                    return true;

                default:
                    return true;
            }

        case 2:
            switch (number)
            {
                case 0:
                    return false;

                case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 10:
                    op.branches = true;
                    return true;

                case 8: case 9: case 15: case 16: case 17: case 18: case 19:
                case 20: case 21: case 22: case 23: case 24:
                    op.stores = true;
                    return true;

                case 25:
                    op.stores = true;
                    op.isCall = true;
                    return v >= 4;

                case 26:
                    op.isCall = true;
                    return v >= 5;

                case 27:
                    return v >= 5;

                case 28:
                    op.ends = true;
                    return v >= 5;

                case 29: case 30: case 31:
                    return false;

                default:
                    return true;
            }

        default:
            switch (number)
            {
                case 0: case 12:
                    op.stores = true;
                    op.isCall = true;
                    return true;

                case 4:
                    op.stores = v >= 5;
                    return true;

                case 7: case 22:
                    op.stores = true;
                    return true;

                case 9:
                    op.stores = v == 6;
                    return true;

                case 23:
                    op.stores   = true;
                    op.branches = true;
                    return true;

                case 24:
                    op.stores = true;
                    return v >= 5;

                case 25: case 26:
                    op.isCall = true;
                    return v >= 5;

                case 31:
                    op.branches = true;
                    return v >= 5;

                default:
                    return true;
            }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory::Quote
//
//  A string in quotes, each new line as ^, as Inform writes one.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ZMachineStory::Quote (const std::wstring & text)
{
    std::wstring  quoted = L"\"";



    for (wchar_t c : text)
    {
        quoted += (c == L'\n') ? L'^' : c;
    }

    return quoted + L"\"";
}
