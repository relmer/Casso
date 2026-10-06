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
            Assert::AreEqual    (std::wstring (L"Slot 5 ROM"),    MemoryEditModel::GetRegionLabel (slot5));
            Assert::AreEqual    (std::wstring (L"Slot 6 ROM"),    MemoryEditModel::GetRegionLabel (slot6));
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

            Assert::AreEqual (MemoryEditModel::kRegionIo, ReadRegion (model, 0xC0FF));
            Assert::AreEqual (MemoryEditModel::GetRegionKey (MemoryRegion::SlotRom, 0xC100), ReadRegion (model, 0xC100));
            Assert::AreEqual ((uint16_t) 0, ReadRegion (model, 0x0300), L"a byte the window never read has a region");
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
