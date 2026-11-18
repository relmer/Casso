#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/RomSymbols.h"
#include "Debugger/SymbolTable.h"
#include "Machines/Apple3/Apple3Symbols.h"
#include "Machines/Apple3/SosCallNames.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SymbolsTests
//
//  Each SOS call's name by number, and the Apple /// symbol table: it loads
//  as a debug file, has the registers at their addresses, and is the table
//  the Apple /// machines get.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SymbolsTests)
{
public:

    TEST_METHOD (SosCallNames_FirstAndLastOfEachGroup)
    {
        Assert::AreEqual ("CREATE",      SosCallNames::GetName (0xC0));
        Assert::AreEqual ("GET_LEVEL",   SosCallNames::GetName (0xD3));
        Assert::AreEqual ("D_STATUS",    SosCallNames::GetName (0x82));
        Assert::AreEqual ("D_INFO",      SosCallNames::GetName (0x85));
        Assert::AreEqual ("REQUEST_SEG", SosCallNames::GetName (0x40));
        Assert::AreEqual ("RELEASE_SEG", SosCallNames::GetName (0x45));
        Assert::AreEqual ("SET_FENCE",   SosCallNames::GetName (0x60));
        Assert::AreEqual ("TERMINATE",   SosCallNames::GetName (0x65));
        Assert::AreEqual ("READ",        SosCallNames::GetName (0xCA));
        Assert::AreEqual ("GET_TIME",    SosCallNames::GetName (0x63));
    }


    TEST_METHOD (SosCallNames_OtherNumbersAreNotCalls)
    {
        for (Byte number : { 0x00, 0x3F, 0x46, 0x5F, 0x66, 0x81, 0x86, 0xBF, 0xD4, 0xFF })
        {
            Assert::IsNull (SosCallNames::GetName (number), std::format (L"${:02X}", number).c_str());
        }
    }


    TEST_METHOD (Table_LoadsWithTheRegistersAtTheirAddresses)
    {
        SymbolTable  table;
        size_t       loaded  = 0;
        std::string  error;
        Word         address = 0;
        HRESULT      hr      = table.LoadFrom (SymbolTableId::Main, Apple3Symbols::GetMain(), 0, loaded, error);



        Assert::AreEqual (S_OK, hr, TextEncoding::Utf8ToWide (error).c_str());
        Assert::IsTrue   (loaded > 0);

        for (const auto & [name, at] : { std::pair<const char *, Word> { "KBD",     0xC000 },
                                               std::pair<const char *, Word> { "ZPREG",   0xFFD0 },
                                               std::pair<const char *, Word> { "ENVREG",  0xFFDF },
                                               std::pair<const char *, Word> { "BANKREG", 0xFFEF },
                                               std::pair<const char *, Word> { "Q7H",     0xC0EF } })
        {
            Assert::IsTrue   (table.TryResolveIn (SymbolTableId::Main, name, address), TextEncoding::Utf8ToWide (name).c_str());
            Assert::AreEqual (at, address, TextEncoding::Utf8ToWide (name).c_str());
        }
    }


    TEST_METHOD (TheAppleIiiMachines_GetTheAppleIiiTable)
    {
        Assert::AreEqual (Apple3Symbols::GetMain(), RomSymbols::GetMain ("Apple ///"));
        Assert::AreEqual (Apple3Symbols::GetMain(), RomSymbols::GetMain ("Apple /// Plus"));
        Assert::AreNotEqual (Apple3Symbols::GetMain(), RomSymbols::GetMain ("Apple ][+"));
        Assert::AreNotEqual (Apple3Symbols::GetMain(), RomSymbols::GetMain ("Apple //e"));
        Assert::AreNotEqual (Apple3Symbols::GetMain(), RomSymbols::GetMain ("Apple //c"));
    }
};
