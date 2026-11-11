#include "Pch.h"

#include "Ui/Debugger/MemoryFileDialog.h"
#include "Ui/Debugger/ByteDialog.h"
#include "Debugger/DebugMemoryImage.h"
#include "Ui/Debugger/TextDialog.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/FieldDialogPanel.h"
#include "Ui/Debugger/Panes/MemoryPane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryFileDialogTests
    //
    //  The fields of File > Load file into memory... and Save memory to
    //  file...: the BLOAD or BSAVE they make, and the reason for fields that
    //  make none.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MemoryFileDialogTests)
    {
    public:
        using Format = MemoryFileDialog::Format;
        using Fields = MemoryFileDialog::Fields;

        static bool MakeLoad (const Fields & fields, DebuggerAction & action, std::wstring & error)
        {
            return MemoryFileDialog::TryMakeLoad ("game.bin", fields, CommandMode::AppleWin, action, error);
        }



        TEST_METHOD (HexIsOneToFourDigitsWithOrWithoutADollarSign)
        {
            uint32_t  value = 0;



            Assert::IsTrue   (MemoryFileDialog::TryParseHex (L" $6000 ", 4, value));
            Assert::AreEqual (0x6000u, value);
            Assert::IsTrue   (MemoryFileDialog::TryParseHex (L"c0", 4, value));
            Assert::AreEqual (0xC0u, value);
            Assert::IsTrue   (MemoryFileDialog::TryParseHex (L"10000", 5, value), L"a length of all memory");
            Assert::IsFalse  (MemoryFileDialog::TryParseHex (L"10000", 4, value), L"no five-digit address");
            Assert::IsFalse  (MemoryFileDialog::TryParseHex (L"$", 4, value));
            Assert::IsFalse  (MemoryFileDialog::TryParseHex (L"60g0", 4, value));
        }



        TEST_METHOD (ALoadIsBloadInItsFormatAtItsAddress)
        {
            DebuggerAction  action;
            std::wstring    error;



            Assert::IsTrue   (MakeLoad ({ Format::Detect, L"", L"" }, action, error), L"the format and address from the file");
            Assert::AreEqual (std::string ("BLOAD \"game.bin\""), action.echo);
            Assert::IsFalse  (action.command.hasA1);

            Assert::IsTrue   (MakeLoad ({ Format::Dos33, L"$4000", L"" }, action, error), L"in place of the file's address");
            Assert::AreEqual (std::string ("game.bin,DOS"), action.command.text);
            Assert::AreEqual ((Word) 0x4000, action.command.a1);

            Assert::IsTrue   (MakeLoad ({ Format::Raw, L"300", L"$100" }, action, error));
            Assert::AreEqual (std::string ("BLOAD \"game.bin\",RAW 0300,100"), action.echo);
            Assert::AreEqual ((Word) 0x03FF, action.command.a2, L"the last address");
        }



        TEST_METHOD (FieldsThatMakeNoLoadSayWhy)
        {
            DebuggerAction  action;
            std::wstring    error;



            Assert::IsFalse (MakeLoad ({ Format::Raw, L"", L"" }, action, error), L"no address for raw bytes");
            Assert::IsTrue  (error.find (L"address") != std::wstring::npos, error.c_str());

            Assert::IsFalse (MakeLoad ({ Format::IntelHex, L"$300", L"" }, action, error), L"an address for a HEX file");
            Assert::IsFalse (MakeLoad ({ Format::Detect, L"", L"$10" }, action, error), L"a length with no address");
            Assert::IsFalse (MakeLoad ({ Format::Raw, L"$FF00", L"$200" }, action, error), L"past the end of memory");
            Assert::IsFalse (MakeLoad ({ Format::Raw, L"$300", L"0" }, action, error), L"nothing to load");
            Assert::IsFalse (MakeLoad ({ Format::Raw, L"lots", L"" }, action, error));
        }



        TEST_METHOD (APushedByteIsOneOrTwoHexDigits)
        {
            Byte  value = 0;



            Assert::IsTrue   (ByteDialog::TryParseByte (L" $a9 ", value));
            Assert::AreEqual ((Byte) 0xA9, value);
            Assert::IsTrue   (ByteDialog::TryParseByte (L"7", value));
            Assert::AreEqual ((Byte) 0x07, value);
            Assert::IsFalse  (ByteDialog::TryParseByte (L"100", value), L"more than a byte");
            Assert::IsFalse  (ByteDialog::TryParseByte (L"", value));
            Assert::IsFalse  (ByteDialog::TryParseByte (L"zz", value));
        }



        TEST_METHOD (TypedBytesAndAddressesAreHex)
        {
            std::vector<Byte>  bytes;
            Word               address = 0;



            Assert::IsTrue   (TextDialog::TryParseBytes (L" A9 $20  7 ", bytes));
            Assert::IsTrue   ((std::vector<Byte> { 0xA9, 0x20, 0x07 }) == bytes);
            Assert::IsFalse  (TextDialog::TryParseBytes (L"", bytes), L"at least one");
            Assert::IsFalse  (TextDialog::TryParseBytes (L"A9 1234", bytes), L"bytes only");

            Assert::IsTrue   (TextDialog::TryParseAddress (L"$6000", address));
            Assert::AreEqual ((Word) 0x6000, address);
            Assert::IsFalse  (TextDialog::TryParseAddress (L"10000", address));
        }



        TEST_METHOD (TheSpeedReadsAsTheEmulatorsMenuWritesIt)
        {
            Assert::AreEqual (std::wstring (L"1x"),      DebuggerStatusText::GetSpeedText (SpeedMode::Authentic));
            Assert::AreEqual (std::wstring (L"2x"),      DebuggerStatusText::GetSpeedText (SpeedMode::Double));
            Assert::AreEqual (std::wstring (L"Maximum"), DebuggerStatusText::GetSpeedText (SpeedMode::Maximum));
        }



        TEST_METHOD (ASaveIsBsaveOfARangeThatNeedsBothFields)
        {
            DebuggerAction  action;
            std::wstring    error;



            Assert::IsTrue   (MemoryFileDialog::TryMakeSave ("page.bin", { Format::Raw, L"$400", L"$400" }, CommandMode::AppleWin, action, error));
            Assert::AreEqual (std::string ("BSAVE \"page.bin\" 0400,400"), action.echo);
            Assert::AreEqual ((Word) 0x07FF, action.command.a2);

            Assert::IsFalse  (MemoryFileDialog::TryMakeSave ("page.bin", { Format::Raw, L"$400", L"" }, CommandMode::AppleWin, action, error), L"no length");
            Assert::IsFalse  (MemoryFileDialog::TryMakeSave ("page.bin", { Format::Raw, L"", L"$10" }, CommandMode::AppleWin, action, error), L"no address");
        }


        //  A small dialog's label column fits its longest label on one line,
        //  as Add pointer watch's Zero-page address. A short label has the
        //  usual column.
        TEST_METHOD (TheLabelColumnFitsTheLongestLabel)
        {
            FieldDialogPanel  shortPanel;
            FieldDialogPanel  longPanel;
            DxuiLabel         byte;
            DxuiLabel         address;
            DxuiLabel         field;
            DxuiLabel         other;
            float             oneLine = 0.0f;
            float             height  = 0.0f;
            HRESULT           hrOne   = S_OK;
            HRESULT           hrFit   = S_OK;



            byte.SetText    (L"Byte");
            address.SetText (L"Zero-page address of the pointer");
            shortPanel.Add  (byte, field);
            longPanel.Add   (address, other);

            Assert::AreEqual (FieldDialogPanel::kLabelDip, shortPanel.GetLabelDip(), L"a short label");

            hrOne = address.MeasureWrappedDip (10000.0f, oneLine);
            hrFit = address.MeasureWrappedDip ((float) longPanel.GetLabelDip(), height);

            Assert::IsTrue (SUCCEEDED (hrOne) && SUCCEEDED (hrFit));
            Assert::IsTrue (longPanel.GetLabelDip() > FieldDialogPanel::kLabelDip, L"a long label widens the column");
            Assert::AreEqual (oneLine, height, 0.01f, L"on one line");
        }


        //  Add pointer watch's dialog has a description above its field, and
        //  the field is laid out under it.
        TEST_METHOD (ThePointerDialogsDescriptionIsAboveItsField)
        {
            FieldDialogPanel  panel;
            DxuiLabel         description;
            DxuiLabel         label;
            DxuiTextInput     field;
            DxuiDpiScaler     scaler;



            scaler.SetDpi (96);
            description.SetText (ByteDialog::kPointerWatchText);
            label.SetText       (L"Zero-page address");
            panel.SetDescription (description);
            panel.Add            (label, field);
            panel.Layout         (RECT { 0, 0, 320, 200 }, scaler);

            Assert::IsTrue (std::wstring (ByteDialog::kPointerWatchText).find (L"$2000") != std::wstring::npos, L"the example");
            Assert::IsTrue (description.GetRect().bottom > description.GetRect().top + FieldDialogPanel::kRowDip, L"wrapped onto lines");
            Assert::IsTrue (field.GetRect().top >= description.GetRect().bottom, L"the field is under it");
        }


        //  The word a byte and the one after it make, little-endian, wrapping at
        //  the top of memory; none where a byte wasn't read.
        TEST_METHOD (TheWordAtAByteIsAPointer)
        {
            auto                owned  = std::make_unique<DebugMemoryImage>();
            DebugMemoryImage  & memory = *owned;



            memory.bytes[0x3E]   = 0x00;
            memory.bytes[0x3F]   = 0x20;
            memory.bytes[0xFFFF] = 0x34;
            memory.bytes[0x0000] = 0x12;
            memory.readable.set (0x3E);
            memory.readable.set (0x3F);
            memory.readable.set (0xFFFF);
            memory.readable.set (0x0000);

            Assert::AreEqual ((int) 0x2000, (int) MemoryPane::GetWordAt (memory, 0x3E).value_or (0));
            Assert::AreEqual ((int) 0x1234, (int) MemoryPane::GetWordAt (memory, 0xFFFF).value_or (0), L"wraps to $0000");
            Assert::IsFalse  (MemoryPane::GetWordAt (memory, 0x3F).has_value(), L"$40 wasn't read");
        }


        //  The pointer dialog's example: the address $3E and $3F form and the
        //  first bytes there, an I/O byte as --; nothing without memory.
        TEST_METHOD (ThePointerExampleReadsTheAddressAndItsBytes)
        {
            auto                owned  = std::make_unique<DebugMemoryImage>();
            DebugMemoryImage  & memory = *owned;



            memory.bytes[0x3E]   = 0x00;
            memory.bytes[0x3F]   = 0x20;
            memory.bytes[0x2000] = 0xA9;
            memory.bytes[0x2001] = 0x00;
            memory.bytes[0x2002] = 0x8D;
            memory.readable.set();

            Assert::AreEqual (std::wstring (L"$3E: $2000: A9 00 8D ..."), ByteDialog::FormatPointerExample (0x3E, &memory));

            memory.bytes[0x3E] = 0x00;
            memory.bytes[0x3F] = 0xC0;
            memory.readable.reset (0xC001);
            Assert::AreEqual (std::wstring (L"$3E: $C000: 00 -- 00 ..."), ByteDialog::FormatPointerExample (0x3E, &memory));

            Assert::IsTrue (ByteDialog::FormatPointerExample (0x3E, nullptr).empty());
        }
    };
}
