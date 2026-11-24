#include "Pch.h"
#include "CassoExplorer/CassoExplorerActions.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoToTests
//
//  The Go to, Search and number fields: addresses, offsets and ranges, hex
//  bytes and quoted text, and a search across chunks of a large source.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ActionsGoToTests)
{
public:

    TEST_METHOD (Address_ParsesTheUsualForms)
    {
        Word  address = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseAddress (L"$2000", address));
        Assert::AreEqual ((int) 0x2000, (int) address);
        Assert::IsTrue   (ActionsGoTo::TryParseAddress (L" 0x0803 ", address));
        Assert::AreEqual ((int) 0x0803, (int) address);
        Assert::IsTrue   (ActionsGoTo::TryParseAddress (L"#768", address));
        Assert::AreEqual ((int) 768, (int) address);
        Assert::IsFalse  (ActionsGoTo::TryParseAddress (L"$10000", address));
        Assert::IsFalse  (ActionsGoTo::TryParseAddress (L"zz", address));
        Assert::IsFalse  (ActionsGoTo::TryParseAddress (L"", address));
    }


    TEST_METHOD (GoTo_ParsesAddressesAndOffsetsFromTheCaret)
    {
        int64_t  address = 0;
        int64_t  last    = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"$08FF", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x08FF, address);
        Assert::AreEqual (address, last, L"One address is a range of one");
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"+10", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x0810, address, L"An offset is hex, as addresses are");
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"+$A0", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x08A0, address);
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L" -#16 ", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x07F0, address, L"and # marks decimal");
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"+", 0x0800, address, last));
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"", 0x0800, address, last));
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"zz", 0x0800, address, last));
    }


    TEST_METHOD (GoTo_ReachesPastSixteenBitsInALargeFile)
    {
        int64_t  first = 0;
        int64_t  last  = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"10000", 0, first, last));
        Assert::AreEqual ((int64_t) 0x10000, first, L"An offset is not an Apple II address and is not capped at $FFFF");
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"+#1000000", 0x10, first, last));
        Assert::AreEqual ((int64_t) 1000016, first);
    }


    TEST_METHOD (GoTo_ParsesARangeByLastAddressOrByLength)
    {
        int64_t  first = 0;
        int64_t  last  = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"$0803-$0810", 0x0800, first, last));
        Assert::AreEqual ((int64_t) 0x0803, first);
        Assert::AreEqual ((int64_t) 0x0810, last, L"A hyphen gives the last address");

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"$0803,+10", 0x0800, first, last));
        Assert::AreEqual ((int64_t) 0x0812, last, L"A comma and a plus give a length of sixteen");

        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"$0810-$0803", 0x0800, first, last),
            L"A range cannot end before it starts");
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"$0803,+0", 0x0800, first, last),
            L"or be empty");
    }


    TEST_METHOD (Search_ParsesHexBytesAndQuotedText)
    {
        std::vector<Byte>  bytes;
        bool               isText = false;

        Assert::IsTrue  (ActionsGoTo::TryParseSearch (L" A9 00 8d ", bytes, isText));
        Assert::IsFalse (isText);
        Assert::IsTrue  (bytes == std::vector<Byte> { 0xA9, 0x00, 0x8D });

        Assert::IsTrue  (ActionsGoTo::TryParseSearch (L"\"HI\"", bytes, isText));
        Assert::IsTrue  (isText);
        Assert::IsTrue  (bytes == std::vector<Byte> { 'H', 'I' });

        Assert::IsTrue  (ActionsGoTo::TryParseSearch (L"ZZ", bytes, isText));
        Assert::IsTrue  (isText, L"What is not hex digits is text");
        Assert::IsTrue  (bytes == std::vector<Byte> { 'Z', 'Z' });
        Assert::IsFalse (ActionsGoTo::TryParseSearch (L"\"\"", bytes, isText));
        Assert::IsFalse (ActionsGoTo::TryParseSearch (L"", bytes, isText));
    }


    TEST_METHOD (Search_FindsForwardWrapsAndMatchesTextEitherWay)
    {
        std::vector<Byte>  haystack = { 0x01, 0xC8, 0xC9, 0x02, 0x48, 0x49 };

        //  "hi" matches high-bit "HI" at 1, then plain "HI" at 4, then wraps.
        Assert::AreEqual ((size_t) 1, ActionsGoTo::FindBytes (haystack, { 'h', 'i' }, true, 0));
        Assert::AreEqual ((size_t) 4, ActionsGoTo::FindBytes (haystack, { 'h', 'i' }, true, 2));
        Assert::AreEqual ((size_t) 1, ActionsGoTo::FindBytes (haystack, { 'h', 'i' }, true, 5));

        //  Bytes match exactly.
        Assert::AreEqual ((size_t) 4, ActionsGoTo::FindBytes (haystack, { 0x48 }, false, 0));
        Assert::AreEqual (CassoExplorerActions::kNotFound, ActionsGoTo::FindBytes (haystack, { 0x77 }, false, 0));
    }


    TEST_METHOD (Search_InSourceFindsAMatchAcrossTwoChunksAndWraps)
    {
        std::vector<Byte>       haystack (CassoExplorerActions::kSearchChunkBytes * 2 + 100, (Byte) 0);
        uint64_t                across = CassoExplorerActions::kSearchChunkBytes - 2;
        CassoExplorerActions::ReadFn  read   = [&haystack] (uint64_t offset, std::span<uint8_t> out)
        {
            std::copy_n (haystack.begin() + (ptrdiff_t) offset, out.size(), out.begin());
        };

        haystack[(size_t) across]     = 0xDE;
        haystack[(size_t) across + 1] = 0xAD;
        haystack[(size_t) across + 2] = 0xBE;
        haystack[(size_t) across + 3] = 0xEF;

        Assert::AreEqual (across, ActionsGoTo::FindInSource (read, haystack.size(), { 0xDE, 0xAD, 0xBE, 0xEF }, false, 0),
            L"A match that straddles the first chunk's end is found");
        Assert::AreEqual (across, ActionsGoTo::FindInSource (read, haystack.size(), { 0xDE, 0xAD, 0xBE, 0xEF }, false, across + 1),
            L"and a search past it wraps back to it");
        Assert::AreEqual (CassoExplorerActions::kNotFoundOffset, ActionsGoTo::FindInSource (read, haystack.size(), { 0x77 }, false, 0));
    }


    TEST_METHOD (Numbers_ParseSpacesAndCommas)
    {
        std::vector<int>  numbers;

        Assert::IsTrue   (ActionsGoTo::TryParseNumbers (L"17 0 2", 2, numbers));
        Assert::AreEqual ((size_t) 3, numbers.size());
        Assert::AreEqual (17, numbers[0]);
        Assert::IsTrue   (ActionsGoTo::TryParseNumbers (L"17,0", 2, numbers));
        Assert::IsFalse  (ActionsGoTo::TryParseNumbers (L"17", 2, numbers));
        Assert::IsFalse  (ActionsGoTo::TryParseNumbers (L"1 2 3 4", 1, numbers));
        Assert::IsFalse  (ActionsGoTo::TryParseNumbers (L"17 x", 1, numbers));
    }

};
