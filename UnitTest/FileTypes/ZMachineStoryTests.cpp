#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/ZMachineStory.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/InfocomVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStoryTests
//
//  A version 3 story built here by the Z-Machine Standards Document 1.1:
//  an abbreviation, two objects, a dictionary, a first instruction that
//  prints and calls, a routine its property and the call both point to, and
//  two strings after the code.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ZMachineStoryTests)
{
public:

    static constexpr size_t  kAbbreviations = 0x0040;
    static constexpr size_t  kGlobals       = 0x0100;
    static constexpr size_t  kObjects       = 0x02E0;
    static constexpr size_t  kFirstObject   = kObjects + 31 * 2;
    static constexpr size_t  kRoomTable     = kFirstObject + 2 * 9;
    static constexpr size_t  kDictionary    = 0x0380;
    static constexpr size_t  kTheString     = 0x03C0;
    static constexpr size_t  kCode          = 0x0400;

    //  Z-characters for text: a letter in A0, a capital shifted to A1, the
    //  space, and punctuation shifted to A2.
    static std::vector<int> ToZchars (const std::string & text)
    {
        static const std::string  kA2 = " \n0123456789.,!?_#'\"/\\-:()";

        std::vector<int>  zchars;



        for (char c : text)
        {
            if (c == ' ')
            {
                zchars.push_back (0);
            }
            else if (c >= 'a' && c <= 'z')
            {
                zchars.push_back (6 + c - 'a');
            }
            else if (c >= 'A' && c <= 'Z')
            {
                zchars.push_back (4);
                zchars.push_back (6 + c - 'A');
            }
            else
            {
                zchars.push_back (5);
                zchars.push_back (6 + (int) kA2.find (c));
            }
        }

        return zchars;
    }

    //  Three Z-characters a word, padded with 5s, the last word's top bit set.
    static std::vector<Byte> Encode (std::vector<int> zchars, size_t minimumWords = 1)
    {
        std::vector<Byte>  bytes;



        while (zchars.size() % 3 != 0 || zchars.size() < minimumWords * 3)
        {
            zchars.push_back (5);
        }

        for (size_t at = 0; at < zchars.size(); at += 3)
        {
            Word  word = (Word) ((zchars[at] << 10) | (zchars[at + 1] << 5) | zchars[at + 2]);

            if (at + 3 == zchars.size())
            {
                word |= 0x8000;
            }

            bytes.push_back ((Byte) (word >> 8));
            bytes.push_back ((Byte) word);
        }

        return bytes;
    }

    static void PutWord (std::vector<Byte> & bytes, size_t at, size_t value)
    {
        bytes[at]     = (Byte) (value >> 8);
        bytes[at + 1] = (Byte) value;
    }

    static void Put (std::vector<Byte> & bytes, size_t at, const std::vector<Byte> & part)
    {
        std::copy (part.begin(), part.end(), bytes.begin() + (ptrdiff_t) at);
    }

    static std::vector<Byte> MakeStory()
    {
        std::vector<Byte>  story (0x500, 0);
        std::vector<Byte>  hello    = Encode ({ 4, 13, 10, 17, 17, 20, 0, 1, 0 });   // "Hello " then abbreviation 0
        std::vector<Byte>  lampOn   = Encode (ToZchars ("Lamp on"));
        std::vector<Byte>  goodbye  = Encode (ToZchars ("Goodbye"));
        std::vector<Byte>  accented = Encode ({ 5, 6, 5, 10 });                    // ZSCII 170, an e with an acute accent
        size_t             at       = kCode;
        size_t             routine  = 0;
        size_t             strings  = 0;
        uint32_t           sum      = 0;



        story[0] = 3;
        PutWord (story, 0x02, 88);
        PutWord (story, 0x04, kCode);
        PutWord (story, 0x06, kCode);
        PutWord (story, 0x08, kDictionary);
        PutWord (story, 0x0A, kObjects);
        PutWord (story, 0x0C, kGlobals);
        PutWord (story, 0x0E, kDictionary);
        PutWord (story, 0x18, kAbbreviations);
        Put     (story, 0x12, { '8', '4', '0', '7', '2', '6' });

        //  Abbreviation 0 is "the ".
        PutWord (story, kAbbreviations, kTheString / 2);
        Put     (story, kTheString, Encode (ToZchars ("the ")));

        //  The first instruction: print "Hello the ", call the routine, print
        //  the string after the code, and quit.
        story[at++] = 0xB2;
        Put (story, at, hello);
        at += hello.size();

        routine = 0x440;
        strings = routine + 2 + lampOn.size();

        story[at++] = 0xE0;
        story[at++] = 0x3F;
        PutWord (story, at, routine / 2);
        at += 2;
        story[at++] = 0x00;

        story[at++] = 0x8D;
        PutWord (story, at, strings / 2);
        at += 2;

        story[at++] = 0xBA;

        //  The routine: no locals, then print_ret "Lamp on".
        story[routine]     = 0;
        story[routine + 1] = 0xB3;
        Put (story, routine + 2, lampOn);

        Put (story, strings, goodbye);
        Put (story, strings + goodbye.size() + (goodbye.size() % 2), accented);

        //  Object 1, the room, is the parent of object 2, the lamp, whose
        //  property 5 is the routine's address.
        story[kFirstObject]         = 0x80;         // attribute 0
        story[kFirstObject + 6]     = 2;            // child
        PutWord (story, kFirstObject + 7, kRoomTable);
        story[kFirstObject + 9 + 4] = 1;            // parent
        PutWord (story, kFirstObject + 9 + 7, kRoomTable + 8);

        story[kRoomTable] = 2;
        Put (story, kRoomTable + 1, Encode (ToZchars ("room"), 2));
        story[kRoomTable + 5] = 0;

        story[kRoomTable + 8] = 2;
        Put (story, kRoomTable + 9, Encode (ToZchars ("lamp"), 2));
        story[kRoomTable + 13] = (Byte) ((1 << 5) | 5);
        PutWord (story, kRoomTable + 14, routine / 2);
        story[kRoomTable + 16] = 0;

        //  Two separators, entries of 7 bytes, and two words.
        Put (story, kDictionary, { 2, '.', ',', 7, 0, 2 });
        Put (story, kDictionary + 6,  Encode (ToZchars ("lamp"), 2));
        Put (story, kDictionary + 13, Encode (ToZchars ("take"), 2));

        PutWord (story, 0x1A, story.size() / 2);

        for (size_t b = 0x40; b < story.size(); b++)
        {
            sum += story[b];
        }

        PutWord (story, 0x1C, sum & 0xFFFF);

        return story;
    }

    static bool HasLine (const DecodedFile & file, const std::wstring & text)
    {
        return std::any_of (file.lines.begin(), file.lines.end(), [&text] (const std::wstring & line) { return line.find (text) != std::wstring::npos; });
    }

    static DecodedFile DecodeStory (const std::vector<Byte> & story)
    {
        FileFacts      facts;
        DecodedFile    file;
        std::wstring   error;



        AssertSucceeded (ZMachineStory::Decode (story, facts, DecodeOptions(), file, error));

        return file;
    }


    TEST_METHOD (Matches_AStoryWhoseChecksumMatches_OrNamedForTheMachine)
    {
        std::vector<Byte>  story = MakeStory();
        FileFacts          facts;



        Assert::IsTrue (ZMachineStory::IsVerifiedStory (story));
        Assert::IsTrue (ZMachineStory::Matches (facts, story));

        story[0x41] ^= 0xFF;

        Assert::IsFalse (ZMachineStory::Matches (facts, story), L"A changed byte breaks the checksum");

        facts.name = "ZORK.Z3";

        Assert::IsTrue  (ZMachineStory::Matches (facts, story), L"and the name marks it");
        Assert::IsFalse (ZMachineStory::Matches (facts, std::vector<Byte> (0x500, 0)));
    }


    TEST_METHOD (Header_IsListed)
    {
        DecodedFile  file = DecodeStory (MakeStory());



        Assert::AreEqual (std::wstring (L"Z-machine story, version 3"), file.lines[0]);
        Assert::IsTrue   (HasLine (file, L"Release 88, serial number 840726"));
        Assert::IsTrue   (HasLine (file, L", which matches"));
        Assert::IsTrue   (HasLine (file, L"Dictionary $0380"));
    }


    TEST_METHOD (AbbreviationsAndDictionary_AreDecoded)
    {
        DecodedFile  file = DecodeStory (MakeStory());



        Assert::IsTrue (HasLine (file, L"   0  \"the \""));
        Assert::IsTrue (HasLine (file, L"Dictionary: 2 words, separators . ,"));
        Assert::IsTrue (HasLine (file, L"  $0386  lamp"));
        Assert::IsTrue (HasLine (file, L"  $038D  take"));
    }


    TEST_METHOD (Objects_AreListedWithTheirTree)
    {
        DecodedFile  file = DecodeStory (MakeStory());



        Assert::IsTrue (HasLine (file, L"Objects: 2"));
        Assert::IsTrue (HasLine (file, L"    1  \"room\""));
        Assert::IsTrue (HasLine (file, L"parent 1, sibling 0, child 0"));
        Assert::IsTrue (HasLine (file, L"attributes 0"));
        Assert::IsTrue (HasLine (file, L"properties 5:02 20"));
        Assert::IsTrue (HasLine (file, L"  [1] room"));
        Assert::IsTrue (HasLine (file, L"    [2] lamp"));
    }


    TEST_METHOD (Strings_TheCodePrintsAndThoseAfterIt)
    {
        DecodedFile  file = DecodeStory (MakeStory());



        Assert::IsTrue (HasLine (file, L"\"Hello the \""), L"An abbreviation expands");
        Assert::IsTrue (HasLine (file, L"\"Lamp on\""),    L"The routine's text");
        Assert::IsTrue (HasLine (file, L"\"Goodbye\""),    L"print_paddr's string");
        Assert::IsTrue (HasLine (file, L"\"\x00E9\""),     L"The string after it, an escaped character");
    }

    //  An Infocom disk: the story from track 3, its sectors in a skewed
    //  order, is the volume's one file.
    TEST_METHOD (InfocomDisk_ItsStoryIsItsOneFile)
    {
        static constexpr int  kSkew[16] = { 0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9 };

        std::vector<Byte>         story   = MakeStory();
        std::vector<Byte>         disk (143360, 0);
        std::unique_ptr<IVolume>  volume;
        VolumeListing             listing;
        FilePayload               payload;
        std::vector<Byte>         ignored;
        HRESULT                   hrDelete = S_OK;



        for (size_t sector = 0; sector * 256 < story.size(); sector++)
        {
            size_t  track = 3 + sector / 16;
            size_t  unit  = track * 16 + (size_t) kSkew[sector % 16];
            size_t  count = std::min ((size_t) 256, story.size() - sector * 256);

            std::copy (story.begin() + (ptrdiff_t) (sector * 256), story.begin() + (ptrdiff_t) (sector * 256 + count), disk.begin() + (ptrdiff_t) (unit * 256));
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (disk) == VolumeKind::Infocom);

        volume = VolumeImage::CreateVolume (VolumeKind::Infocom, disk);

        AssertSucceeded (volume->Enumerate (listing));
        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::AreEqual (std::string ("STORY.Z3"), listing.entries[0].name);

        AssertSucceeded (volume->Read (FilePath::Parse ("STORY.Z3"), payload));
        Assert::IsTrue (payload.bytes == story);

        hrDelete = volume->Delete (FilePath::Parse ("STORY.Z3"), ignored);

        Assert::IsTrue (FAILED (hrDelete), L"It's read-only");
    }


    TEST_METHOD (BlankDisk_IsNoInfocomDisk)
    {
        Assert::IsFalse (InfocomVolume::IsFound (std::vector<Byte> (143360, 0)));
    }
};