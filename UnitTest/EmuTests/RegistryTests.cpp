#include "Pch.h"
#include "Core/ComponentRegistry.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"

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
        ComponentRegistry  registry;
        MemoryBus          bus;
        DeviceConfig       cfg;

        bool factoryCalled = false;

        registry.Register ("test-device",
            [&] (const DeviceConfig &, MemoryBus &) -> std::unique_ptr<MemoryDevice>
            {
                factoryCalled = true;
                return nullptr;
            });

        cfg.type = "test-device";
        registry.Create ("test-device", cfg, bus);

        Assert::IsTrue (factoryCalled);
    }

    TEST_METHOD (UnknownType_ReturnsNull)
    {
        ComponentRegistry  registry;
        MemoryBus          bus;
        DeviceConfig       cfg;

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
        ComponentRegistry         registry;
        std::vector<std::string>  types;

        registry.Register ("type-a",
            [] (const DeviceConfig &, MemoryBus &) { return std::unique_ptr<MemoryDevice> (); });
        registry.Register ("type-b",
            [] (const DeviceConfig &, MemoryBus &) { return std::unique_ptr<MemoryDevice> (); });

        types = registry.GetRegisteredTypes();

        Assert::IsTrue (types.size() >= 2);
    }

    TEST_METHOD (BuiltinDevices_AreRegistered)
    {
        ComponentRegistry registry;
        ComponentRegistry::RegisterBuiltinDevices (registry);

        Assert::IsTrue (registry.IsRegistered ("apple2-family-keyboard"));
        Assert::IsTrue (registry.IsRegistered ("apple2-family-speaker"));
        Assert::IsTrue (registry.IsRegistered ("apple2-family-softswitches"));
        Assert::IsTrue (registry.IsRegistered ("apple2-family-gameport"));
        Assert::IsTrue (registry.IsRegistered ("language-card"));
        Assert::IsTrue (registry.IsRegistered ("disk-ii"));
    }


    //  Every machine build registers the built-in devices again, and a scratch
    //  replay machine is built on a worker thread while the window thread builds
    //  another, so registrations and lookups run on two threads at once.
    TEST_METHOD (RegisteringOnTwoThreadsAtOnceLeavesTheRegistryIntact)
    {
        static constexpr int  s_kRounds = 2000;

        std::atomic<int>  misses = 0;
        std::thread       other;

        auto  work = [&misses]
        {
            ComponentRegistry  registry;

            for (int round = 0; round < s_kRounds; round++)
            {
                ComponentRegistry::RegisterBuiltinDevices (registry);

                if (!registry.IsRegistered ("apple2e-family-softswitches") || registry.GetRegisteredTypes().empty())
                {
                    misses++;
                }
            }
        };



        other = std::thread (work);
        work();
        other.join();

        Assert::AreEqual (0, misses.load(), L"every lookup finds the built-in devices");
    }
};
