#include "Pch.h"

#include "AssetBootstrap.h"
#include "Config/StockMachines.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachinesTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StockMachinesTests)
{
public:

    TEST_METHOD (GetAll_EveryModelHasAnEmbeddedConfig)
    {
        for (const StockMachine & machine : StockMachines::GetAll())
        {
            Assert::IsTrue (AssetBootstrap::HasEmbeddedConfig (TextEncoding::NarrowToWide (machine.id)),
                            TextEncoding::NarrowToWide (machine.id).c_str());
        }
    }

    TEST_METHOD (GetAll_ListsTheModelsInOrder)
    {
        std::vector<std::string>  ids;
        std::vector<std::string>  listed = { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c", "Apple2cPlus" };



        for (const StockMachine & machine : StockMachines::GetAll())
        {
            ids.push_back (machine.id);
        }

        Assert::IsTrue (ids == listed);
    }

    TEST_METHOD (GetDefault_IsTheEnhancedIIe)
    {
        Assert::AreEqual (std::string ("Apple2eEnhanced"), std::string (StockMachines::GetDefault().id));
    }

    TEST_METHOD (Find_MatchesWithoutCase)
    {
        const StockMachine  * machine = StockMachines::Find ("apple2CPLUS");



        Assert::IsNotNull (machine);
        Assert::AreEqual (std::string ("Apple2cPlus"), std::string (machine->id));
        Assert::AreEqual (std::wstring (L"Apple //c Plus"), std::wstring (machine->label));
    }

    TEST_METHOD (Find_UnknownOrPrefix_ReturnsNull)
    {
        Assert::IsNull (StockMachines::Find ("Apple3"));
        Assert::IsNull (StockMachines::Find ("Apple2c2"));
        Assert::IsNull (StockMachines::Find ("Apple"));
        Assert::IsNull (StockMachines::Find (""));
    }

    TEST_METHOD (Find_SlotsAndCpu)
    {
        Assert::IsTrue  (StockMachines::Find ("Apple2")->is6502);
        Assert::IsTrue  (StockMachines::Find ("Apple2Plus")->is6502);
        Assert::IsFalse (StockMachines::Find ("Apple2e")->is6502);
        Assert::IsTrue  (StockMachines::Find ("Apple2eEnhanced")->hasSlots);
        Assert::IsFalse (StockMachines::Find ("Apple2c")->hasSlots);
        Assert::IsFalse (StockMachines::Find ("Apple2cPlus")->hasSlots);
    }

    TEST_METHOD (FormatUnknown_ListsEveryModel)
    {
        Assert::AreEqual (std::wstring (L"Unknown machine 'Foo'. Expected Apple2, Apple2Plus, Apple2e, Apple2eEnhanced, Apple2c or Apple2cPlus."),
                          StockMachines::FormatUnknown ("Foo"));
    }
};
