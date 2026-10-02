#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Common/PrinterCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPrinterStateTests
//
//  Save and load of the 6551 ACIA behind the //c serial ports and of the
//  parallel printer card. The ACIA round trip fills every saved register of a
//  source and a target with different values and compares every one after the
//  load; the run test receives and reads bytes after a save and checks that a
//  load puts the ACIA back to answer the same way.
//
////////////////////////////////////////////////////////////////////////////////

namespace SerialPrinterState
{
    static constexpr Word  kAciaBase = static_cast<Word> (Acia6551::kSlotIoBase + 2 * Acia6551::kSlotIoStride + Acia6551::kAciaRegOffset);
    static constexpr int   kSlot     = 1;


    template <typename T>
    static std::vector<Byte> Save (const T & part)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = part.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    // Loads a blob into a part; on success the part must have consumed it all.
    static HRESULT LoadFrom (IMachineState & target, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);
        HRESULT      hr = S_OK;



        hr = target.LoadState (reader);

        if (SUCCEEDED (hr))
        {
            Assert::IsTrue (reader.IsAtEnd());
        }

        return hr;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeAcia
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeAcia : public Acia6551
    {
    public:
        StateProbeAcia() : Acia6551 (kAciaBase) {}

        void Fill (Byte seed)
        {
            m_status  = static_cast<Byte> (seed + 1);
            m_command = static_cast<Byte> (seed + 2);
            m_control = static_cast<Byte> (seed + 3);
            m_rxData  = static_cast<Byte> (seed + 4);
        }

        void AssertSameState (const StateProbeAcia & other) const
        {
            Assert::AreEqual (m_status,  other.m_status,  L"status");
            Assert::AreEqual (m_command, other.m_command, L"command");
            Assert::AreEqual (m_control, other.m_control, L"control");
            Assert::AreEqual (m_rxData,  other.m_rxData,  L"receive data");
        }

        // Receives two bytes and reads back the data and status registers
        // after each.
        std::vector<Byte> Exchange()
        {
            std::vector<Byte>  out;



            ReceiveByte (0x41);
            out.push_back (Read (static_cast<Word> (kAciaBase + kRegStatus)));
            out.push_back (Read (static_cast<Word> (kAciaBase + kRegData)));

            ReceiveByte (0x42);
            ReceiveByte (0x43);
            out.push_back (Read (static_cast<Word> (kAciaBase + kRegStatus)));
            out.push_back (Read (static_cast<Word> (kAciaBase + kRegData)));
            out.push_back (Read (static_cast<Word> (kAciaBase + kRegStatus)));

            return out;
        }
    };


    TEST_CLASS (SerialPrinterStateTests)
    {
    public:
        TEST_METHOD (AciaRoundTripsEveryField)
        {
            StateProbeAcia     source;
            StateProbeAcia     target;
            HRESULT            hr = S_OK;



            source.Fill (0x35);
            target.Fill (0xCA);

            hr = LoadFrom (target, Save (source));
            Assert::AreEqual (S_OK, hr);

            source.AssertSameState (target);
        }


        TEST_METHOD (AciaRunsTheSameAfterLoad)
        {
            StateProbeAcia     acia;
            StateProbeAcia     fresh;
            std::vector<Byte>  saved;
            std::vector<Byte>  expected;
            HRESULT            hr = S_OK;



            acia.Write (static_cast<Word> (kAciaBase + Acia6551::kRegControl), 0x1E);
            acia.Write (static_cast<Word> (kAciaBase + Acia6551::kRegCommand), Acia6551::kCommandDtr);
            acia.ReceiveByte (0x7F);

            saved    = Save (acia);
            expected = acia.Exchange();

            hr = LoadFrom (acia, saved);
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (expected == acia.Exchange(), L"the same ACIA loaded back");

            hr = LoadFrom (fresh, saved);
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (expected == fresh.Exchange(), L"a fresh ACIA loaded from the save");
            Assert::IsTrue   (Save (acia) == Save (fresh));
        }


        TEST_METHOD (PrinterRoundTripsTouched)
        {
            auto     touched = std::make_unique<PrinterCard> (kSlot);   // heap: each card embeds a 64KB ring (C6262)
            auto     fresh   = std::make_unique<PrinterCard> (kSlot);
            auto     other   = std::make_unique<PrinterCard> (kSlot);
            HRESULT  hr      = S_OK;



            touched->Write (touched->GetStart(), 'A');

            hr = LoadFrom (*fresh, Save (*touched));
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (fresh->HasBeenTouched());

            hr = LoadFrom (*touched, Save (*other));
            Assert::AreEqual (S_OK, hr);
            Assert::IsFalse  (touched->HasBeenTouched());
        }
    };
}
