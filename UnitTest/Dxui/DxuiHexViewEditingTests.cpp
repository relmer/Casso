#include "Pch.h"

#include "Widgets/DxuiHexView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewEditingTests
//
//  Typing into an editable hex view (FR-035): hex digits collect for the value
//  under the caret and are written the moment the value is complete, a
//  character in the text column is written at once, and the caret moves on.
//
//  THE SOURCE DECIDES. The view hands a completed value to the source and
//  moves on only if the source took it; a source that refuses (ROM it cannot
//  patch, an I/O address) leaves the caret where it was and says so. A source
//  that never overrides WriteBytes is read-only, which keeps every existing
//  view, Cassque's included, exactly as it was.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiHexViewEditingTests
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  WritableSource
    //
    //  Sixty-four zero bytes that record every write and refuse one offset.
    //
    ////////////////////////////////////////////////////////////////////////////

    class WritableSource : public IDxuiHexSource
    {
    public:
        struct Write
        {
            uint64_t              offset;
            std::vector<uint8_t>  bytes;
        };

        mutable std::vector<uint8_t>  bytes   = std::vector<uint8_t> (64, 0);
        mutable std::vector<Write>    writes;
        uint64_t                      refused = UINT64_MAX;

        uint64_t  GetByteCount() const override { return bytes.size(); }

        void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
        {
            std::copy_n (bytes.begin() + (ptrdiff_t) offset, out.size(), out.begin());
        }

        bool  WriteBytes (uint64_t offset, std::span<const uint8_t> data) const override
        {
            if (offset == refused)
            {
                return false;
            }

            writes.push_back ({ offset, std::vector<uint8_t> (data.begin(), data.end()) });
            std::copy (data.begin(), data.end(), bytes.begin() + (ptrdiff_t) offset);
            return true;
        }
    };



    class ReadOnlySource : public IDxuiHexSource
    {
    public:
        uint64_t  GetByteCount() const override { return 64; }
        void      ReadBytes (uint64_t, std::span<uint8_t> out) const override { std::fill (out.begin(), out.end(), uint8_t (0)); }
    };



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rig
    //
    ////////////////////////////////////////////////////////////////////////////

    struct Rig
    {
        WritableSource  source;
        DxuiHexView     view;

        Rig (int grouping = 1)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            view.SetSource      (&source);
            view.SetCellSizeDip (8, 16);
            view.SetGrouping    (grouping);
            view.SetEditable    (true);
            view.Layout         (RECT { 0, 0, 800, 320 }, scaler);
            view.GoToOffset     (0);
        }

        bool  Type (const wchar_t * chars)
        {
            bool  consumed = true;

            for (const wchar_t * at = chars; *at != L'\0'; ++at)
            {
                DxuiKeyEvent  ev;

                ev.kind  = DxuiKeyEventKind::Char;
                ev.vk    = (WPARAM) *at;
                consumed = view.OnKey (ev) && consumed;
            }

            return consumed;
        }

        bool  Press (WPARAM vk)
        {
            DxuiKeyEvent  ev;

            ev.kind = DxuiKeyEventKind::Down;
            ev.vk   = vk;
            return view.OnKey (ev);
        }
    };





    TEST_CLASS (HexColumnTests)
    {
    public:

        TEST_METHOD (TwoDigitsWriteAByteAndMoveOn)
        {
            Rig  rig;



            rig.view.GoToOffset (3);
            Assert::IsTrue   (rig.Type (L"a9"));

            Assert::AreEqual ((size_t) 1,       rig.source.writes.size());
            Assert::AreEqual ((uint64_t) 3,     rig.source.writes[0].offset);
            Assert::AreEqual ((uint8_t) 0xA9,   rig.source.writes[0].bytes.at (0));
            Assert::AreEqual ((uint64_t) 4,     rig.view.GetCaret(), L"the caret moves to the next byte");
        }


        TEST_METHOD (OneDigitWritesNothingYet)
        {
            Rig  rig;



            Assert::IsTrue   (rig.Type (L"4"));
            Assert::IsTrue   (rig.source.writes.empty());
            Assert::AreEqual (std::wstring (L"4"), rig.view.GetPendingDigits());
            Assert::AreEqual ((uint64_t) 0, rig.view.GetCaret());
        }


        TEST_METHOD (ARunOfBytesTypesStraightThrough)
        {
            Rig  rig;



            rig.Type (L"A94160");

            Assert::AreEqual ((uint8_t) 0xA9, rig.source.bytes[0]);
            Assert::AreEqual ((uint8_t) 0x41, rig.source.bytes[1]);
            Assert::AreEqual ((uint8_t) 0x60, rig.source.bytes[2]);
            Assert::AreEqual ((uint64_t) 3,   rig.view.GetCaret());
        }


        TEST_METHOD (AWordIsWrittenLowByteFirst)
        {
            Rig  rig (2);



            rig.Type (L"1234");

            Assert::AreEqual ((size_t) 1,     rig.source.writes.size(), L"one write for the whole value");
            Assert::AreEqual ((uint8_t) 0x34, rig.source.bytes[0]);
            Assert::AreEqual ((uint8_t) 0x12, rig.source.bytes[1]);
            Assert::AreEqual ((uint64_t) 2,   rig.view.GetCaret(), L"and the caret moves to the next word");
        }


        TEST_METHOD (EscapeAbandonsAPartValue)
        {
            Rig  rig;



            rig.Type (L"7");
            Assert::IsTrue   (rig.Press (VK_ESCAPE));
            Assert::AreEqual (std::wstring(), rig.view.GetPendingDigits());

            rig.Type (L"5");
            Assert::AreEqual (std::wstring (L"5"), rig.view.GetPendingDigits(), L"the next digit starts afresh");
        }


        TEST_METHOD (MovingTheCaretAbandonsAPartValue)
        {
            Rig  rig;



            rig.Type  (L"7");
            rig.Press (VK_RIGHT);

            Assert::AreEqual (std::wstring(), rig.view.GetPendingDigits());
            Assert::IsTrue   (rig.source.writes.empty());
        }


        TEST_METHOD (ANonHexCharacterIsNotTaken)
        {
            Rig  rig;



            Assert::IsFalse (rig.Type (L"g"));
            Assert::IsTrue  (rig.view.GetPendingDigits().empty());
        }


        TEST_METHOD (ARefusedWriteStaysPutAndSaysWhere)
        {
            Rig       rig;
            uint64_t  refusedAt = UINT64_MAX;



            rig.source.refused = 0;
            rig.view.SetOnWriteRefused ([&] (uint64_t offset) { refusedAt = offset; });

            rig.Type (L"FF");

            Assert::AreEqual ((uint64_t) 0, refusedAt);
            Assert::AreEqual ((uint64_t) 0, rig.view.GetCaret(),      L"the caret stays on the cell that refused");
            Assert::IsTrue   (rig.view.GetPendingDigits().empty(),    L"and the digits are dropped");
        }
    };





    TEST_CLASS (TextColumnTests)
    {
    public:

        TEST_METHOD (ACharacterIsWrittenAtOnce)
        {
            Rig  rig;



            rig.view.SetActiveColumn (DxuiHexView::Column::Text);
            rig.Type (L"HI");

            Assert::AreEqual ((uint8_t) 'H',  rig.source.bytes[0]);
            Assert::AreEqual ((uint8_t) 'I',  rig.source.bytes[1]);
            Assert::AreEqual ((uint64_t) 2,   rig.view.GetCaret());
        }


        TEST_METHOD (AppleTextSetsTheHighBit)
        {
            Rig  rig;



            rig.view.SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
            rig.view.SetActiveColumn (DxuiHexView::Column::Text);
            rig.Type (L"A");

            Assert::AreEqual ((uint8_t) 0xC1, rig.source.bytes[0], L"a normal Apple character, as the screen shows it");
        }


        TEST_METHOD (AControlCharacterIsNotTaken)
        {
            Rig  rig;



            rig.view.SetActiveColumn (DxuiHexView::Column::Text);
            Assert::IsFalse (rig.Type (L"\x01"));
            Assert::IsTrue  (rig.source.writes.empty());
        }
    };





    TEST_CLASS (ReadOnlyTests)
    {
    public:

        TEST_METHOD (NotEditableByDefault)
        {
            WritableSource  source;
            DxuiHexView     view;
            DxuiKeyEvent    ev;



            view.SetSource (&source);
            ev.kind = DxuiKeyEventKind::Char;
            ev.vk   = L'A';

            Assert::IsFalse (view.IsEditable());
            Assert::IsFalse (view.OnKey (ev));
            Assert::IsTrue  (source.writes.empty());
        }


        TEST_METHOD (ASourceWithoutWriteBytesRefusesEveryEdit)
        {
            ReadOnlySource  source;
            DxuiHexView     view;
            DxuiDpiScaler   scaler;
            uint64_t        refusedAt = UINT64_MAX;
            DxuiKeyEvent    ev;



            scaler.SetDpi (96);
            view.SetSource        (&source);
            view.SetCellSizeDip   (8, 16);
            view.SetEditable      (true);
            view.Layout           (RECT { 0, 0, 800, 320 }, scaler);
            view.SetOnWriteRefused ([&] (uint64_t offset) { refusedAt = offset; });

            ev.kind = DxuiKeyEventKind::Char;

            for (wchar_t ch : { L'1', L'2' })
            {
                ev.vk = (WPARAM) ch;
                (void) view.OnKey (ev);
            }

            Assert::AreEqual ((uint64_t) 0, refusedAt);
        }
    };
}
