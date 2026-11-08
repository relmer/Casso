#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/MemoryPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionOutlineTests
//
//  The regions a memory window outlines: ROM, I/O, the language card's banks
//  and aux RAM each a region of its own, every slot's ROM apart from the
//  next, each labeled and colored as its bytes or the memory map are, and
//  main RAM in none. The window turns the hex view's outlines on.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryRegionOutlineTests)
    {
    public:

        static constexpr int  kPage = 256;


        static void  Show (MemoryEditModel & model, Word first, std::initializer_list<MemoryRegion> pages)
        {
            std::vector<std::optional<Byte>>  bytes;
            std::vector<MemoryRegion>         regions;

            for (MemoryRegion region : pages)
            {
                for (int i = 0; i < kPage; i++)
                {
                    bytes.push_back (region == MemoryRegion::Io ? std::optional<Byte>() : std::optional<Byte> ((Byte) i));
                    regions.push_back (region);
                }
            }

            model.SetContents (first, bytes, regions);
        }


        static uint16_t  ReadRegion (const MemoryEditModel & model, Word address)
        {
            uint16_t  region = 0xFFFF;

            model.ReadRegions (address, std::span<uint16_t> (&region, 1));
            return region;
        }


        TEST_METHOD (EachSlotsRomIsARegionOfItsOwn)
        {
            uint16_t  slot5 = MemoryEditModel::GetRegionKey (MemoryRegion::SlotRom, 0xC500);
            uint16_t  slot6 = MemoryEditModel::GetRegionKey (MemoryRegion::SlotRom, 0xC6FF);



            Assert::AreNotEqual (slot5, slot6);
            Assert::AreEqual    (std::wstring (L"Slot 5: empty"), MemoryEditModel::GetRegionLabel (slot5));
            Assert::AreEqual    (std::wstring (L"Slot 6: empty"), MemoryEditModel::GetRegionLabel (slot6));
            Assert::AreEqual    (std::wstring (L"Expansion ROM"), MemoryEditModel::GetRegionLabel (MemoryEditModel::GetRegionKey (MemoryRegion::SlotRom, 0xC800)));
        }


        TEST_METHOD (EveryOtherRegionHasItsLabelAndMainRamHasNone)
        {
            Assert::AreEqual (std::wstring (L"ROM"),       MemoryEditModel::GetRegionLabel (MemoryEditModel::GetRegionKey (MemoryRegion::Rom,     0xF800)));
            Assert::AreEqual (std::wstring (L"I/O"),       MemoryEditModel::GetRegionLabel (MemoryEditModel::GetRegionKey (MemoryRegion::Io,      0xC000)));
            Assert::AreEqual (std::wstring (L"LC bank 1"), MemoryEditModel::GetRegionLabel (MemoryEditModel::GetRegionKey (MemoryRegion::LcBank1, 0xD000)));
            Assert::AreEqual (std::wstring (L"LC bank 2"), MemoryEditModel::GetRegionLabel (MemoryEditModel::GetRegionKey (MemoryRegion::LcBank2, 0xD000)));
            Assert::AreEqual (std::wstring (L"Aux RAM"),   MemoryEditModel::GetRegionLabel (MemoryEditModel::GetRegionKey (MemoryRegion::AuxRam,  0x0400)));
            Assert::AreEqual ((uint16_t) 0,                MemoryEditModel::GetRegionKey (MemoryRegion::MainRam, 0x0400));
        }


        TEST_METHOD (TheShownBytesGiveTheirRegionsAndUnreadOnesNone)
        {
            MemoryEditModel  model;



            Show (model, 0xC000, { MemoryRegion::Io, MemoryRegion::SlotRom });

            Assert::AreEqual (MemoryEditModel::kRegionIo, ReadRegion (model, 0xC07F));
            Assert::AreEqual ((uint16_t) (MemoryEditModel::kRegionSlotIo + 7), ReadRegion (model, 0xC0FF), L"slot 7's I/O is a separate range");
            Assert::AreEqual (MemoryEditModel::GetRegionKey (MemoryRegion::SlotRom, 0xC100), ReadRegion (model, 0xC100));
            Assert::AreEqual ((uint16_t) 0, ReadRegion (model, 0x0300), L"a byte the window never read has a region");
        }


        //  A slot's ROM page and its I/O are labeled with the card in it; an
        //  empty slot's with its number, and slot 0's I/O is the language
        //  card's.
        TEST_METHOD (ASlotsRangesAreLabeledWithItsCard)
        {
            MemoryRegionKeys::SlotCards  cards;



            cards[1] = "Parallel printer";
            cards[2] = "Super Serial Card";

            Assert::AreEqual (std::wstring (L"Slot 2: Super Serial Card"),   MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::SlotRom, 0xC200), cards));
            Assert::AreEqual (std::wstring (L"Slot 2: Super Serial Card"),   MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::Io,      0xC0AF), cards));
            Assert::AreEqual (std::wstring (L"Slot 1: parallel printer"),    MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::SlotRom, 0xC100), cards), L"a common noun in sentence case");
            Assert::AreEqual (std::wstring (L"Slot 4: empty"),               MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::SlotRom, 0xC400), cards));
            Assert::AreEqual (std::wstring (L"Slot 4: empty"),               MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::Io,      0xC0C0), cards));
            Assert::AreEqual (std::wstring (L"Slot 0: language card switches"), MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::Io,      0xC080), cards));
            Assert::AreEqual (std::wstring (L"I/O"),                         MemoryRegionKeys::GetLabel (MemoryRegionKeys::GetKey (MemoryRegion::Io,      0xC07F), cards));
            Assert::IsTrue   (MemoryRegionKeys::IsIo  (MemoryRegionKeys::GetKey (MemoryRegion::Io,      0xC0AF)), L"a slot's I/O is drawn as I/O");
            Assert::IsTrue   (MemoryRegionKeys::IsRom (MemoryRegionKeys::GetKey (MemoryRegion::SlotRom, 0xC200)), L"a slot's page as ROM");
        }


        //  A ROM byte's label has the part of the ROM, by the machine; a slot's
        //  ROM page has the card in the slot.
        TEST_METHOD (ARomLabelGivesThePart)
        {
            MemoryRegionKeys::SlotCards  cards;
            std::string                  machine = "Apple2eEnhanced";



            cards[6] = "Disk ][";

            Assert::AreEqual (std::wstring (L"Monitor"),            MemoryRegionKeys::GetRomArea ("Apple2Plus",      0xFDED));
            Assert::AreEqual (std::wstring (L"Applesoft BASIC"),    MemoryRegionKeys::GetRomArea ("Apple2e",         0xD000));
            Assert::AreEqual (std::wstring (L"Integer BASIC"),      MemoryRegionKeys::GetRomArea ("Apple2",          0xE000));
            Assert::IsTrue   (MemoryRegionKeys::GetRomArea ("Apple2", 0xD000).empty(), L"no part of the original ROM there");
            Assert::AreEqual (std::wstring (L"80-column firmware"), MemoryRegionKeys::GetRomArea ("Apple2eEnhanced", 0xC300));
            Assert::AreEqual (std::wstring (L"80-column firmware"), MemoryRegionKeys::GetRomArea ("Apple2eEnhanced", 0xC800));
            Assert::AreEqual (std::wstring (L"internal firmware"),  MemoryRegionKeys::GetRomArea ("Apple2c",         0xC300));

            Assert::AreEqual (std::wstring (L"ROM: Applesoft BASIC"),
                              MemoryRegionKeys::GetRomLabel (MemoryRegionKeys::kRegionRom, 0xE000, cards, machine));
            Assert::AreEqual (std::wstring (L"Slot 6: Disk ]["),
                              MemoryRegionKeys::GetRomLabel (MemoryRegionKeys::GetKey (MemoryRegion::SlotRom, 0xC600), 0xC600, cards, machine));
            Assert::AreEqual (std::wstring (L"LC bank 2"),
                              MemoryRegionKeys::GetRomLabel (MemoryRegionKeys::kRegionLcBank2, 0xD000, cards, machine), L"no ROM behind it");
            Assert::AreEqual (std::wstring (L"ROM: Applesoft BASIC, Monitor"),
                              MemoryRegionKeys::GetRangeLabel (MemoryRegionKeys::kRegionRom, 0xD000, 0x3000, cards, machine), L"a range's parts, each once");
        }


        //  The system ROM is outlined in its parts, each labeled with the part.
        TEST_METHOD (AWindowOutlinesEachPartOfTheRom)
        {
            MemoryEditModel  model;
            uint32_t         argb  = 0;
            uint16_t         basic = 0;
            uint16_t         mon   = 0;
            std::wstring     label;



            model.SetMachineId ("Apple2eEnhanced");
            Show (model, 0xF700, { MemoryRegion::Rom, MemoryRegion::Rom });

            basic = ReadRegion (model, 0xF7FF);
            mon   = ReadRegion (model, 0xF800);

            Assert::AreNotEqual (basic, mon, L"Applesoft and the Monitor are one outline");
            Assert::IsTrue      (model.TryGetRegionStyle (basic, argb, label));
            Assert::AreEqual    (std::wstring (L"ROM: Applesoft BASIC"), label);
            Assert::IsTrue      (model.TryGetRegionStyle (mon, argb, label));
            Assert::AreEqual    (std::wstring (L"ROM: Monitor"), label);
        }


        TEST_METHOD (OutlinesTakeTheirBytesAndTheMapsColors)
        {
            MemoryEditModel                 model;
            MemoryEditModel::RegionColors   colors  = { 0xFF000001, 0xFF000002, 0xFF000003, 0xFF000004, 0xFF000005 };
            uint32_t                        argb    = 0;
            std::wstring                    label;



            model.SetRegionColors (colors);

            Assert::IsTrue   (model.TryGetRegionStyle (MemoryEditModel::kRegionIo, argb, label));
            Assert::AreEqual (colors.io, argb);
            Assert::IsTrue   (model.TryGetRegionStyle (MemoryEditModel::GetRegionKey (MemoryRegion::SlotRom, 0xC600), argb, label));
            Assert::AreEqual (colors.rom, argb, L"a slot's ROM is not drawn in ROM's color");
            Assert::IsTrue   (model.TryGetRegionStyle (MemoryEditModel::kRegionLcBank2, argb, label));
            Assert::AreEqual (colors.lcBank2, argb);
            Assert::IsTrue   (model.TryGetRegionStyle (MemoryEditModel::kRegionAux, argb, label));
            Assert::AreEqual (colors.aux, argb);
            Assert::IsFalse  (model.TryGetRegionStyle (0, argb, label), L"main RAM is outlined");
        }


        TEST_METHOD (TheWindowTurnsOutlinesOnInTheThemesColors)
        {
            DxuiHexView              view;
            CassoTheme               theme  = CassoTheme::MakeSkeuomorphic();
            DebuggerTextColors::Set  colors = DebuggerTextColors::MakeFor (theme);
            MemoryPane               pane (1, &view, [] (int, Word) {}, [] (const DebuggerActionBuilder &) {}, [] (const std::string &) {});
            uint32_t                 argb   = 0;
            std::wstring             label;



            pane.Configure     (nullptr);
            pane.SetTextColors (colors);

            Assert::IsTrue   (view.IsShowingRegions(), L"the memory window left the outlines off");
            Assert::IsTrue   (view.GetSource()->TryGetRegionStyle (MemoryEditModel::kRegionRom, argb, label));
            Assert::AreEqual (colors.rom, argb);
            Assert::IsTrue   (view.GetSource()->TryGetRegionStyle (MemoryEditModel::kRegionLcBank1, argb, label));
            Assert::AreEqual (colors.mapLcBank1, argb);
        }
    };
}
