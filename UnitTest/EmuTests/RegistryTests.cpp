#include "../Casso65Emu/Pch.h"

#include <CppUnitTest.h>

#include "../Casso65Emu/Core/ComponentRegistry.h"
#include "../Casso65Emu/Core/MachineConfig.h"
#include "../Casso65Emu/Core/MemoryBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RegistryTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RegistryTests)
{
public:

    TEST_METHOD (Register_DeviceFactory_CanCreate)
    {
        ComponentRegistry registry;
        MemoryBus bus;

        bool factoryCalled = false;

        registry.Register ("test-device",
            [&] (const DeviceConfig &, MemoryBus &) -> std::unique_ptr<MemoryDevice>
            {
                factoryCalled = true;
                return nullptr;
            });

        DeviceConfig cfg;
        cfg.type = "test-device";
        registry.Create ("test-device", cfg, bus);

        Assert::IsTrue (factoryCalled);
    }

    TEST_METHOD (UnknownType_ReturnsNull)
    {
        ComponentRegistry registry;
        MemoryBus bus;
        DeviceConfig cfg;

        auto result = registry.Create ("nonexistent", cfg, bus);

        Assert::IsNull (result.get ());
    }

    TEST_METHOD (IsRegistered_ReturnsTrueForKnown)
    {
        ComponentRegistry registry;

        registry.Register ("known",
            [] (const DeviceConfig &, MemoryBus &) -> std::unique_ptr<MemoryDevice>
            {
                return nullptr;
            });

        Assert::IsTrue (registry.IsRegistered ("known"));
        Assert::IsFalse (registry.IsRegistered ("unknown"));
    }

    TEST_METHOD (GetRegisteredTypes_ListsAll)
    {
        ComponentRegistry registry;

        registry.Register ("type-a",
            [] (const DeviceConfig &, MemoryBus &) { return std::unique_ptr<MemoryDevice> (); });
        registry.Register ("type-b",
            [] (const DeviceConfig &, MemoryBus &) { return std::unique_ptr<MemoryDevice> (); });

        auto types = registry.GetRegisteredTypes ();

        Assert::IsTrue (types.size () >= 2);
    }

    TEST_METHOD (BuiltinDevices_AreRegistered)
    {
        ComponentRegistry registry;
        ComponentRegistry::RegisterBuiltinDevices (registry);

        Assert::IsTrue (registry.IsRegistered ("apple2-keyboard"));
        Assert::IsTrue (registry.IsRegistered ("apple2-speaker"));
        Assert::IsTrue (registry.IsRegistered ("apple2-softswitches"));
        Assert::IsTrue (registry.IsRegistered ("language-card"));
        Assert::IsTrue (registry.IsRegistered ("disk-ii"));
    }
};
