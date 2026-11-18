#include "Pch.h"

#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple3/Apple3CharacterSet.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3CharacterSetTests
//
//  The character generator RAM: the download addresses, a download changing
//  only the characters it lists, no change with the download off, the
//  random power-on contents and the saved state.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3CharacterSetTests)
{
public:

    static constexpr size_t  kLowSystemSize = 0x2000;

    using SystemRam = std::array<Byte, kLowSystemSize>;


    static Apple3CharacterSet::SystemReader MakeReader (const SystemRam & ram)
    {
        return [&ram] (Word address) { return ram[address]; };
    }


    //  Lists characters firstCode + cell in the eight cells, each row's dots
    //  the cell number in the low bits and the row number above them.
    static void ListCharacters (SystemRam & ram, Byte firstCode)
    {
        for (int cell = 0; cell < Apple3CharacterSet::kCellCount; cell++)
        {
            for (int row = 0; row < IGlyphSource::kRowCount; row++)
            {
                ram[Apple3CharacterSet::GetCodeAddress (cell, row)] = (Byte) (firstCode + cell);
                ram[Apple3CharacterSet::GetDotsAddress (cell, row)] = (Byte) ((row << 3) | cell);
            }
        }
    }


    TEST_METHOD (DownloadAddresses_AreTheTextPageScreenHoles)
    {
        Assert::AreEqual ((Word) 0x0878, Apple3CharacterSet::GetCodeAddress (0, 0));
        Assert::AreEqual ((Word) 0x087B, Apple3CharacterSet::GetCodeAddress (3, 0));
        Assert::AreEqual ((Word) 0x087C, Apple3CharacterSet::GetCodeAddress (0, 1));
        Assert::AreEqual ((Word) 0x08F8, Apple3CharacterSet::GetCodeAddress (0, 2));
        Assert::AreEqual ((Word) 0x08FC, Apple3CharacterSet::GetCodeAddress (0, 3));
        Assert::AreEqual ((Word) 0x0978, Apple3CharacterSet::GetCodeAddress (0, 4));
        Assert::AreEqual ((Word) 0x0A78, Apple3CharacterSet::GetCodeAddress (4, 0));
        Assert::AreEqual ((Word) 0x0BFF, Apple3CharacterSet::GetCodeAddress (7, 7));

        Assert::AreEqual ((Word) 0x0478, Apple3CharacterSet::GetDotsAddress (0, 0));
        Assert::AreEqual ((Word) 0x07FF, Apple3CharacterSet::GetDotsAddress (7, 7));
    }


    TEST_METHOD (Download_WritesTheListedCharactersOnly)
    {
        SystemRam           ram = {};
        Apple3CharacterSet  set;
        Prng                prng (1);
        Byte                before = 0;



        set.PowerOn (prng);
        before = set.GetGlyphRow (0x20, 0);

        ListCharacters (ram, 0x41);
        set.SetDownloadEnabled (true);
        set.OnVerticalBlank (MakeReader (ram));

        for (int cell = 0; cell < Apple3CharacterSet::kCellCount; cell++)
        {
            for (int row = 0; row < IGlyphSource::kRowCount; row++)
            {
                Assert::AreEqual ((Byte) ((row << 3) | cell), set.GetGlyphRow ((Byte) (0x41 + cell), row));
            }
        }

        Assert::AreEqual (before, set.GetGlyphRow (0x20, 0), L"An unlisted character is not written");
    }


    TEST_METHOD (Download_UsesTheLowSevenBitsOfTheCode)
    {
        SystemRam           ram = {};
        Apple3CharacterSet  set;



        ListCharacters (ram, 0xC1);
        set.SetDownloadEnabled (true);
        set.OnVerticalBlank (MakeReader (ram));

        Assert::AreEqual ((Byte) 0x01, set.GetGlyphRow (0x42, 0));
    }


    TEST_METHOD (DownloadOff_ChangesNothing)
    {
        SystemRam           ram = {};
        Apple3CharacterSet  set;
        Prng                prng (2);
        std::vector<Byte>   before;



        set.PowerOn (prng);

        for (int code = 0; code < IGlyphSource::kCharacterCount; code++)
        {
            before.push_back (set.GetGlyphRow ((Byte) code, 0));
        }

        ListCharacters (ram, 0x41);
        set.OnVerticalBlank (MakeReader (ram));

        set.SetDownloadEnabled (true);
        set.SetDownloadEnabled (false);
        set.OnVerticalBlank (MakeReader (ram));

        for (int code = 0; code < IGlyphSource::kCharacterCount; code++)
        {
            Assert::AreEqual (before[(size_t) code], set.GetGlyphRow ((Byte) code, 0));
        }
    }


    TEST_METHOD (PowerOn_FillsTheRamFromTheSeed)
    {
        Apple3CharacterSet  first;
        Apple3CharacterSet  same;
        Apple3CharacterSet  other;
        Prng                prng1 (7);
        Prng                prng2 (7);
        Prng                prng3 (8);
        bool                isSame    = true;
        bool                isOther   = false;
        bool                isNonZero = false;



        first.SetDownloadEnabled (true);

        first.PowerOn (prng1);
        same.PowerOn  (prng2);
        other.PowerOn (prng3);

        for (int code = 0; code < IGlyphSource::kCharacterCount; code++)
        {
            for (int row = 0; row < IGlyphSource::kRowCount; row++)
            {
                Byte  value = first.GetGlyphRow ((Byte) code, row);



                isSame    = isSame    && value == same.GetGlyphRow ((Byte) code, row);
                isOther   = isOther   || value != other.GetGlyphRow ((Byte) code, row);
                isNonZero = isNonZero || value != 0;
            }
        }

        Assert::IsTrue  (isSame);
        Assert::IsTrue  (isOther);
        Assert::IsTrue  (isNonZero);
        Assert::IsFalse (first.IsDownloadEnabled(), L"Power on turns the download off");
    }


    TEST_METHOD (State_RoundTrips)
    {
        Apple3CharacterSet  set;
        Apple3CharacterSet  loaded;
        Prng                prng (3);
        StateWriter         writer;
        HRESULT             hr = S_OK;



        set.PowerOn (prng);
        set.SetDownloadEnabled (true);

        hr = set.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        StateReader  reader (writer.GetBytes());

        hr = loaded.LoadState (reader);
        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (reader.IsAtEnd());
        Assert::IsTrue   (loaded.IsDownloadEnabled());

        for (int code = 0; code < IGlyphSource::kCharacterCount; code++)
        {
            Assert::AreEqual (set.GetGlyphRow ((Byte) code, 5), loaded.GetGlyphRow ((Byte) code, 5));
        }
    }
};
