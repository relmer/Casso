#include "Pch.h"

#include "Core/JsonParser.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/ReplyJson.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJsonTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ReplyJsonTests)
    {
    public:

        static JsonValue ParseRecord (const std::string & line)
        {
            JsonValue       root;
            JsonParseError  error;
            HRESULT         hr = S_OK;



            Assert::AreEqual (std::string::npos, line.find ('\n'), L"record holds a raw newline");
            hr = JsonParser::Parse (line, root, error);
            Assert::AreEqual (S_OK, hr, std::wstring (line.begin(), line.end()).c_str());
            return root;
        }

        static const JsonValue & GetObjectMember (const JsonValue & value, const std::string & key)
        {
            const JsonValue * member = nullptr;



            Assert::IsTrue (value.HasObject (key, member), std::wstring (key.begin(), key.end()).c_str());
            return *member;
        }

        static const JsonValue & GetArrayMember (const JsonValue & value, const std::string & key)
        {
            const JsonValue * member = nullptr;



            Assert::IsTrue (value.HasArray (key, member), std::wstring (key.begin(), key.end()).c_str());
            return *member;
        }

        static Reply MakeOk (ReplyData data)
        {
            Reply reply;



            reply.command = "cmd";
            reply.data    = std::move (data);
            AppleWinFormatter::Format (reply);
            return reply;
        }



        TEST_METHOD (Reply_BreakpointSet_IntegersAndText)
        {
            BreakpointSetData  set;
            JsonValue          root;
            std::string        type;
            std::string        status;
            int                id = 0;



            set.breakpoint.address = 0x0300;
            set.breakpoint.last    = 0x0300;
            root = ParseRecord (ReplyJson::WriteReply (MakeOk (set), 7));

            Assert::AreEqual (S_OK, root.GetString ("type", type));
            Assert::AreEqual (std::string ("reply"), type);
            Assert::AreEqual (S_OK, root.GetInt ("id", id));
            Assert::AreEqual (7, id);
            Assert::AreEqual (S_OK, root.GetString ("status", status));
            Assert::AreEqual (std::string ("ok"), status);

            const JsonValue & breakpoint = GetObjectMember (GetObjectMember (root, "data"), "breakpoint");
            Assert::AreEqual (S_OK, breakpoint.GetInt ("address", id));
            Assert::AreEqual (768, id);
            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300"), GetArrayMember (root, "text").GetArrayElement (0).GetString());
        }



        TEST_METHOD (Memory_UnreadableByteIsNull)
        {
            MemoryData  data;
            MemoryRow   row;
            JsonValue   root;
            std::string region;
            int         id = 0;



            row.address = 0xC000;
            row.bytes   = { std::nullopt, 0x41 };
            row.region  = MemoryRegion::Io;
            data.rows   = { row };
            root        = ParseRecord (ReplyJson::WriteReply (MakeOk (data), std::nullopt));

            const JsonValue & first = GetArrayMember (GetObjectMember (root, "data"), "rows").GetArrayElement (0);
            const JsonValue & bytes = GetArrayMember (first, "bytes");

            Assert::IsTrue   (bytes.GetArrayElement (0).GetType() == JsonType::Null);
            Assert::AreEqual (65, bytes.GetArrayElement (1).GetInt());
            Assert::AreEqual (S_OK, first.GetString ("region", region));
            Assert::AreEqual (std::string ("io"), region);
            Assert::IsFalse  (root.HasInt ("id", id));
        }



        TEST_METHOD (Disassembly_CarriesLabelAndOperandSymbol)
        {
            DisassemblyData  data;
            DisassemblyLine  line;
            JsonValue        root;
            std::string      text;
            int              value = 0;



            line.instruction.address           = 0x0300;
            line.instruction.bytes             = { 0x91, 0x06 };
            line.instruction.mnemonic          = "STA";
            line.instruction.operand           = "($06),Y";
            line.instruction.hasOperandAddress = true;
            line.instruction.operandAddress    = 0x0006;
            line.label                         = "LOOP";
            line.operandSymbol                 = "PTR";
            data.lines                         = { line };
            root                               = ParseRecord (ReplyJson::WriteReply (MakeOk (data), std::nullopt));

            const JsonValue & first = GetArrayMember (GetObjectMember (root, "data"), "lines").GetArrayElement (0);

            Assert::AreEqual (S_OK, first.GetString ("operand", text));
            Assert::AreEqual (std::string ("($06),Y"), text, L"the operand stays numeric; the symbol is its own field");
            Assert::AreEqual (S_OK, first.GetInt ("operandAddress", value));
            Assert::AreEqual (6, value);
            Assert::AreEqual (S_OK, first.GetString ("operandSymbol", text));
            Assert::AreEqual (std::string ("PTR"), text);
            Assert::AreEqual (S_OK, first.GetString ("label", text));
            Assert::AreEqual (std::string ("LOOP"), text);
        }



        TEST_METHOD (Disassembly_AbsentNamesAreNull)
        {
            DisassemblyData  data;
            DisassemblyLine  line;
            JsonValue        root;
            std::string      record;



            line.instruction.address  = 0x0300;
            line.instruction.bytes    = { 0xA9, 0x06 };
            line.instruction.mnemonic = "LDA";
            line.instruction.operand  = "#$06";
            data.lines                = { line };
            record                    = ReplyJson::WriteReply (MakeOk (data), std::nullopt);
            root                      = ParseRecord (record);

            //  Present and null, so a client can tell "nothing to name" from an
            //  older server that never sent the field.
            Assert::AreNotEqual (std::string::npos, record.find ("\"label\":null"));
            Assert::AreNotEqual (std::string::npos, record.find ("\"operandSymbol\":null"));
            Assert::AreNotEqual (std::string::npos, record.find ("\"operandAddress\":null"));
        }



        TEST_METHOD (Error_OmitsData_HasErrorObject)
        {
            Reply              reply;
            JsonValue          root;
            std::string        label;
            const JsonValue  * data  = nullptr;



            reply.status      = CommandStatus::Error;
            reply.command     = "G";
            reply.error.label = "already running";
            AppleWinFormatter::Format (reply);
            root = ParseRecord (ReplyJson::WriteReply (reply, 3));

            Assert::IsFalse  (root.HasObject ("data", data));
            Assert::AreEqual (S_OK, GetObjectMember (root, "error").GetString ("label", label));
            Assert::AreEqual (std::string ("already running"), label);
        }



        TEST_METHOD (Profile_CarriesRowsPenaltiesAndAddresses)
        {
            ProfileData  data;
            JsonValue    root;
            std::string  text;
            int          value = 0;



            data.isOn         = true;
            data.instructions = 2;
            data.cycles       = 9;
            data.pageCross    = 1;
            data.opcodes      = { { "LDA", "Absolute, X", 2, 8 } };
            data.addresses    = { { 0x0300, "LOOP", 9 } };
            root              = ParseRecord (ReplyJson::WriteReply (MakeOk (data), std::nullopt));

            const JsonValue & body   = GetObjectMember (root, "data");
            const JsonValue & opcode = GetArrayMember (body, "opcodes").GetArrayElement (0);
            const JsonValue & hot    = GetArrayMember (body, "addresses").GetArrayElement (0);

            Assert::AreEqual (S_OK, body.GetString ("kind", text));
            Assert::AreEqual (std::string ("profile"), text);
            Assert::AreEqual (S_OK, opcode.GetString ("mode", text));
            Assert::AreEqual (std::string ("Absolute, X"), text);
            Assert::AreEqual (S_OK, opcode.GetInt ("cycles", value));
            Assert::AreEqual (8, value);
            Assert::AreEqual (S_OK, GetObjectMember (body, "penalties").GetInt ("pageCross", value));
            Assert::AreEqual (1, value);
            Assert::AreEqual (S_OK, hot.GetInt ("address", value));
            Assert::AreEqual (0x0300, value);
            Assert::AreEqual (S_OK, hot.GetString ("symbol", text));
            Assert::AreEqual (std::string ("LOOP"), text);
        }



        TEST_METHOD (CallStack_CarriesFramesBreaksAndTheLastReturn)
        {
            CallStackData   data;
            CallStackFrame  frame;
            CallStackBreak  txs;
            JsonValue       root;
            std::string     text;
            int             value    = 0;
            bool            verified = true;



            frame.callSite   = 0x0806;
            frame.target     = 0x0820;
            frame.stackLevel = 0xFD;
            frame.symbol     = "THREE";
            txs.kind         = CallBreakKind::Txs;
            txs.pc           = 0x0812;
            txs.opcode       = 0x9A;

            data.mechanism = CallStackMechanism::Recorded;
            data.rows.push_back ({ std::nullopt, txs });
            frame.isVerified = false;
            data.rows.push_back ({ frame, std::nullopt });
            frame.note       = "returned past inline parameters";
            data.lastReturn  = frame;
            root             = ParseRecord (ReplyJson::WriteReply (MakeOk (data), std::nullopt));

            const JsonValue & body     = GetObjectMember (root, "data");
            const JsonValue & breakRow = GetArrayMember (body, "rows").GetArrayElement (0);
            const JsonValue & frameRow = GetArrayMember (body, "rows").GetArrayElement (1);

            Assert::AreEqual (S_OK, body.GetString ("kind", text));
            Assert::AreEqual (std::string ("callStack"), text);
            Assert::AreEqual (S_OK, body.GetString ("mechanism", text));
            Assert::AreEqual (std::string ("RECORDED"), text);
            Assert::AreEqual (S_OK, breakRow.GetString ("break", text));
            Assert::AreEqual (std::string ("txs"), text);
            Assert::AreEqual (S_OK, breakRow.GetInt ("pc", value));
            Assert::AreEqual (0x0812, value);
            Assert::AreEqual (S_OK, breakRow.GetString ("text", text));
            Assert::AreEqual (std::string ("TXS at $0812"), text);
            Assert::AreEqual (S_OK, frameRow.GetInt ("callSite", value));
            Assert::AreEqual (0x0806, value);
            Assert::AreEqual (S_OK, frameRow.GetString ("provenance", text));
            Assert::AreEqual (std::string ("recorded"), text);
            Assert::AreEqual (S_OK, frameRow.GetBool ("verified", verified));
            Assert::IsFalse  (verified);
            Assert::AreEqual (S_OK, GetObjectMember (body, "lastReturn").GetString ("note", text));
            Assert::AreEqual (std::string ("returned past inline parameters"), text);
        }



        TEST_METHOD (EveryDataKind_Serializes)
        {
            std::vector<ReplyData> kinds =
            {
                MessageData(), RegistersData(), MemoryData(), DisassemblyData(), BreakpointSetData(), BreakpointListData(),
                WatchListData(), SearchHitsData(), StackData(), SoftSwitchData(), SymbolData(), CyclesData(), ModeData(), FileIoData(),
                CompareData(), DataBlockListData(), VideoInfoData(), BranchRecordData(), ProfileData(), CalcData(), StepFilterData(),
                CallStackData(), CallStackModeData(),
            };

            std::set<std::string>  names;



            Assert::AreEqual (std::variant_size_v<ReplyData>, kinds.size());

            for (const ReplyData & data : kinds)
            {
                JsonValue    root = ParseRecord (ReplyJson::WriteReply (MakeOk (data), 1));
                std::string  kind;



                Assert::AreEqual (S_OK, GetObjectMember (root, "data").GetString ("kind", kind));
                names.insert (kind);
            }

            Assert::AreEqual (kinds.size(), names.size());
        }



        TEST_METHOD (TextArray_MatchesFormatter)
        {
            RegistersData  data;
            Reply          reply;
            JsonValue      root;



            data.registers = { 0x0300, 0x41, 0x01, 0x02, 0xFD, 0x30 };
            reply          = MakeOk (data);
            root           = ParseRecord (ReplyJson::WriteReply (reply, 2));

            const JsonValue & text = GetArrayMember (root, "text");
            Assert::AreEqual (reply.text.size(), text.GetArraySize());

            for (size_t i = 0; i < reply.text.size(); ++i)
            {
                Assert::AreEqual (reply.text[i], text.GetArrayElement (i).GetString());
            }
        }



        TEST_METHOD (Stopped_AndNotifications)
        {
            StopEvent    stop;
            JsonValue    root;
            std::string  reason;
            std::string  access;
            int          pc = 0;



            stop.reason = StopReason::Watchpoint;
            stop.pc     = 0x0803;
            stop.watch  = WatchHit { 1, 0x0400, 0x41, (Byte) 0xA0, WatchAccess::Write, 0x0800, WatchMode::After };
            root        = ParseRecord (ReplyJson::WriteStopped (stop, 9));

            Assert::AreEqual (S_OK, root.GetString ("reason", reason));
            Assert::AreEqual (std::string ("watchpoint"), reason);
            Assert::AreEqual (S_OK, root.GetInt ("pc", pc));
            Assert::AreEqual (0x0803, pc);
            Assert::AreEqual (S_OK, GetObjectMember (root, "watch").GetString ("access", access));
            Assert::AreEqual (std::string ("write"), access);
            Assert::AreEqual (S_OK, GetObjectMember (root, "watch").GetInt ("previous", pc));
            Assert::AreEqual (0xA0, pc);
            Assert::AreEqual (S_OK, GetObjectMember (root, "watch").GetString ("mode", access));
            Assert::AreEqual (std::string ("after"), access);

            stop.watch->previous.reset();
            root = ParseRecord (ReplyJson::WriteStopped (stop, 9));
            Assert::IsFalse (GetObjectMember (root, "watch").HasInt ("previous", pc));

            ParseRecord (ReplyJson::WriteResumed());
            ParseRecord (ReplyJson::WriteReset (true));
            ParseRecord (ReplyJson::WriteMachineChanged ("Apple //c"));
            ParseRecord (ReplyJson::WriteModeChanged (CommandMode::Monitor));
            ParseRecord (ReplyJson::WriteClosing());
        }



        TEST_METHOD (TextWithNewline_IsEscaped)
        {
            Reply reply;



            reply.command = "ECHO";
            reply.text    = { "line one\nline two" };
            ParseRecord (ReplyJson::WriteReply (reply, 1));
        }
    };
}
