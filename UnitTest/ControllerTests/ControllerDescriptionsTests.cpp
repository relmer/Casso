#include "Pch.h"

#include "Seams/Win32ControllerBackend.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerDescriptionsTests
//
//  A controller is listed by the product name Windows reports, which is
//  ambiguous only when two devices report the same one. The number appended
//  there is the only part of the rule that can be exercised without hardware,
//  and it is the part that decides whether a single controller reads as
//  "Xbox Wireless Controller" or as "Xbox Wireless Controller #1".
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllerDescriptionsTests)
    {
    public:

        static ControllerDeviceInfo MakeXInput (int slot, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.unit.unitId     = std::to_string (slot);
            info.unit.source     = ControllerUnitSource::XInputSlot;
            info.description     = description;
            info.xinputSlot      = slot;

            return info;
        }


        static ControllerDeviceInfo MakeDirectInput (const char * unitId, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::DirectInput;
            info.unit.unitId     = unitId;
            info.unit.source     = ControllerUnitSource::Serial;
            info.description     = description;

            return info;
        }


        TEST_METHOD (Disambiguate_EmptyListIsLeftAlone)
        {
            std::vector<ControllerDeviceInfo>  devices;

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (static_cast<size_t> (0), devices.size());
        }


        TEST_METHOD (Disambiguate_DistinctNamesGetNoNumber)
        {
            std::vector<ControllerDeviceInfo>  devices;

            devices.push_back (MakeXInput (0, L"Xbox Series X Controller"));
            devices.push_back (MakeXInput (1, L"Xbox Wireless Controller"));
            devices.push_back (MakeDirectInput ("A1", L"T.16000M"));

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (std::wstring (L"Xbox Series X Controller"), devices[0].description,
                L"two controllers Windows names differently need nothing appended");
            Assert::AreEqual (std::wstring (L"Xbox Wireless Controller"), devices[1].description);
            Assert::AreEqual (std::wstring (L"T.16000M"),                 devices[2].description);
        }


        TEST_METHOD (Disambiguate_TwoIdenticalXInputTakeTheirSlots)
        {
            std::vector<ControllerDeviceInfo>  devices;

            devices.push_back (MakeXInput (0, L"Xbox Wireless Controller"));
            devices.push_back (MakeXInput (2, L"Xbox Wireless Controller"));

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (std::wstring (L"Xbox Wireless Controller #1"), devices[0].description);
            Assert::AreEqual (std::wstring (L"Xbox Wireless Controller #3"), devices[1].description,
                L"an XInput device is numbered by its slot, not by its place in the list");
        }


        TEST_METHOD (Disambiguate_ThreeIdenticalAllGetNumbers)
        {
            std::vector<ControllerDeviceInfo>  devices;

            devices.push_back (MakeDirectInput ("A1", L"USB Gamepad"));
            devices.push_back (MakeDirectInput ("B2", L"USB Gamepad"));
            devices.push_back (MakeDirectInput ("C3", L"USB Gamepad"));

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (std::wstring (L"USB Gamepad #1"), devices[0].description);
            Assert::AreEqual (std::wstring (L"USB Gamepad #2"), devices[1].description);
            Assert::AreEqual (std::wstring (L"USB Gamepad #3"), devices[2].description);
        }


        TEST_METHOD (Disambiguate_OnlyTheCollidingGroupIsNumbered)
        {
            std::vector<ControllerDeviceInfo>  devices;

            devices.push_back (MakeDirectInput ("A1", L"USB Gamepad"));
            devices.push_back (MakeXInput      (0,    L"Xbox Wireless Controller"));
            devices.push_back (MakeDirectInput ("B2", L"USB Gamepad"));

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (std::wstring (L"USB Gamepad #1"),            devices[0].description);
            Assert::AreEqual (std::wstring (L"Xbox Wireless Controller"),  devices[1].description,
                L"a device whose name is unique keeps it while others are numbered");
            Assert::AreEqual (std::wstring (L"USB Gamepad #2"),            devices[2].description);
        }


        // An XInput device holds the number its slot gives it, so the
        // DirectInput device colliding with it must step over that number
        // rather than repeat it.
        TEST_METHOD (Disambiguate_MixedCollisionProducesTwoDistinctNames)
        {
            std::vector<ControllerDeviceInfo>  devices;

            devices.push_back (MakeDirectInput ("A1", L"Generic Controller"));
            devices.push_back (MakeXInput      (0,    L"Generic Controller"));

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (std::wstring (L"Generic Controller #2"), devices[0].description);
            Assert::AreEqual (std::wstring (L"Generic Controller #1"), devices[1].description);
        }


        // The unit key is what preferences and profiles are stored under, so
        // the description changing must not move any of it.
        TEST_METHOD (Disambiguate_UnitKeysAreUntouched)
        {
            std::vector<ControllerDeviceInfo>  devices;

            devices.push_back (MakeXInput (0, L"Xbox Wireless Controller"));
            devices.push_back (MakeXInput (1, L"Xbox Wireless Controller"));

            Win32ControllerBackend::DisambiguateDescriptions (devices);

            Assert::AreEqual (std::string ("0"), devices[0].unit.unitId);
            Assert::AreEqual (std::string ("1"), devices[1].unit.unitId);
            Assert::IsTrue   (devices[0].unit.model.kind == ControllerKind::XInput);
            Assert::AreEqual (0, devices[0].xinputSlot);
            Assert::AreEqual (1, devices[1].xinputSlot);
        }
    };
}
