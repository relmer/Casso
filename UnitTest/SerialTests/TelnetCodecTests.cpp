#include "Pch.h"

#include "Devices/Serial/TelnetCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace TelnetCodecTestNs
{
    static constexpr Byte  kIac          = TelnetCodec::kIac;
    static constexpr Byte  kWill         = TelnetCodec::kWill;
    static constexpr Byte  kWont         = TelnetCodec::kWont;
    static constexpr Byte  kDo           = TelnetCodec::kDo;
    static constexpr Byte  kDont         = TelnetCodec::kDont;
    static constexpr Byte  kSb           = TelnetCodec::kSb;
    static constexpr Byte  kSe           = TelnetCodec::kSe;
    static constexpr Byte  kEcho         = TelnetCodec::kOptionEcho;
    static constexpr Byte  kSga          = TelnetCodec::kOptionSuppressGoAhead;
    static constexpr Byte  kTerminalType = 24;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  TelnetCodecTests
    //
    //  The Telnet layer of the TCP and modem attachments: $FF doubled in the
    //  data, the remote end's negotiation answered and kept from the guest,
    //  and the NVT line ending.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TelnetCodecTests)
    {
    public:
        static std::vector<Byte> Decode (TelnetCodec & codec, std::vector<Byte> wire, std::vector<Byte> * outReplies = nullptr)
        {
            std::vector<Byte>  data;
            std::vector<Byte>  replies;



            codec.Decode (wire, data, replies);

            if (outReplies != nullptr)
            {
                *outReplies = replies;
            }

            return data;
        }


        TEST_METHOD (FFInTheDataIsDoubledOnTheWire)
        {
            TelnetCodec        codec;
            std::vector<Byte>  wire;
            const Byte         data[] = { 'A', 0xFF, 'B' };



            codec.Encode (data, wire);

            Assert::IsTrue (std::vector<Byte> { 'A', 0xFF, 0xFF, 'B' } == wire);
        }


        TEST_METHOD (ADoubledFFFromTheWireIsOneByte)
        {
            TelnetCodec  codec;



            Assert::IsTrue (std::vector<Byte> { 'A', 0xFF, 'B' } == Decode (codec, { 'A', kIac, kIac, 'B' }));
        }


        TEST_METHOD (EchoAndSuppressGoAheadAreAcceptedOnce)
        {
            TelnetCodec        codec;
            std::vector<Byte>  replies;
            std::vector<Byte>  data;



            data = Decode (codec, { kIac, kWill, kEcho, kIac, kWill, kSga, kIac, kDo, kSga }, &replies);

            Assert::IsTrue (data.empty(), L"negotiation is not passed to the guest");
            Assert::IsTrue (std::vector<Byte> { kIac, kDo, kEcho, kIac, kDo, kSga, kIac, kWill, kSga } == replies);

            Decode (codec, { kIac, kWill, kEcho, kIac, kDo, kSga }, &replies);
            Assert::IsTrue (replies.empty(), L"an option already on is not acknowledged again, so negotiation cannot loop");
        }


        TEST_METHOD (OtherOptionsAreRefused)
        {
            TelnetCodec        codec;
            std::vector<Byte>  replies;



            Decode (codec, { kIac, kDo, kTerminalType, kIac, kWill, kTerminalType }, &replies);

            Assert::IsTrue (std::vector<Byte> { kIac, kWont, kTerminalType, kIac, kDont, kTerminalType } == replies);
        }


        TEST_METHOD (TurningAnAcceptedOptionOffIsAcknowledged)
        {
            TelnetCodec        codec;
            std::vector<Byte>  replies;



            Decode (codec, { kIac, kWill, kEcho }, &replies);
            Decode (codec, { kIac, kWont, kEcho }, &replies);

            Assert::IsTrue (std::vector<Byte> { kIac, kDont, kEcho } == replies);

            Decode (codec, { kIac, kWont, kEcho }, &replies);
            Assert::IsTrue (replies.empty(), L"off already: no answer");
        }


        TEST_METHOD (SubnegotiationAndOtherCommandsAreDropped)
        {
            constexpr Byte  kNop  = 241;
            TelnetCodec     codec;



            Assert::IsTrue (std::vector<Byte> { 'x', 'y' } == Decode (codec, { 'x', kIac, kSb, kTerminalType, 1, kIac, kIac, kIac, kSe, kIac, kNop, 'y' }),
                            L"a doubled $FF inside the subnegotiation does not end it");
        }


        TEST_METHOD (ACarriageReturnGoesOutWithANul)
        {
            TelnetCodec        codec;
            std::vector<Byte>  wire;
            const Byte         data[] = { 'A', '\r' };



            codec.Encode (data, wire);

            Assert::IsTrue (std::vector<Byte> { 'A', '\r', 0 } == wire);
        }


        TEST_METHOD (TheNulAfterAReceivedCarriageReturnIsDropped)
        {
            TelnetCodec  codec;



            Assert::IsTrue (std::vector<Byte> { '\r', 'A', '\r', '\n', 0 } == Decode (codec, { '\r', 0, 'A', '\r', '\n', 0 }), L"only a NUL right after CR is dropped");
        }


        TEST_METHOD (ASequenceSplitAcrossReadsIsDecoded)
        {
            TelnetCodec        codec;
            std::vector<Byte>  replies;
            std::vector<Byte>  data;



            data = Decode (codec, { 'a', kIac });
            Assert::IsTrue (std::vector<Byte> { 'a' } == data);

            data = Decode (codec, { kWill }, &replies);
            Assert::IsTrue (data.empty() && replies.empty());

            data = Decode (codec, { kEcho, '\r' }, &replies);
            Assert::IsTrue (std::vector<Byte> { '\r' } == data);
            Assert::IsTrue (std::vector<Byte> { kIac, kDo, kEcho } == replies);

            data = Decode (codec, { 0, 'b' });
            Assert::IsTrue (std::vector<Byte> { 'b' } == data, L"the NUL after a CR in the read before is dropped too");
        }


        TEST_METHOD (ResetForgetsTheOptions)
        {
            TelnetCodec        codec;
            std::vector<Byte>  replies;



            Decode (codec, { kIac, kWill, kEcho });
            codec.Reset();
            Decode (codec, { kIac, kWill, kEcho }, &replies);

            Assert::IsTrue (std::vector<Byte> { kIac, kDo, kEcho } == replies, L"a new connection negotiates again");
        }
    };
}
