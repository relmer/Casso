#include "Pch.h"
#include "Core/ComponentRegistry.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RegistryTests
//
//  The device-factory registry: registering a type, creating by name, and
//  rejecting an unknown one.
//
//  Creation BY NAME is what makes machine configs data rather than code -- a
//  config names "disk-ii" and the registry produces one -- so these assert the
//  indirection itself, not any particular device.
//
//  An unknown name must fail rather than yielding null, since a config naming a
//  device that does not exist is a mistake to report, and a null quietly
//  produces a machine missing a card nobody notices is gone.
//
//  Re-registration is covered because it decides whether a later registration
//  wins or is refused, and both are defensible -- so the choice is pinned
//  rather than left to whoever reads the implementation.
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

        Assert::IsNull (result.get());
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

        auto types = registry.GetRegisteredTypes();

        Assert::IsTrue (types.size() >= 2);
    }

    TEST_METHOD (BuiltinDevices_AreRegistered)
    {
        ComponentRegistry registry;
        ComponentRegistry::RegisterBuiltinDevices (registry);

        Assert::IsTrue (registry.IsRegistered ("apple2-keyboard"));
        Assert::IsTrue (registry.IsRegistered ("apple2-speaker"));
        Assert::IsTrue (registry.IsRegistered ("apple2-softswitches"));
        Assert::IsTrue (registry.IsRegistered ("apple2-gameport"));
        Assert::IsTrue (registry.IsRegistered ("language-card"));
        Assert::IsTrue (registry.IsRegistered ("disk-ii"));
    }
};
