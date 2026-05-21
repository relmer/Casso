#include "Pch.h"

#include "Core/MachineConfigUpgrade.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigUpgradeTests
//
//  Pure unit tests for the embedded-machine-config upgrade planner.
//  The planner has no I/O surface — tests supply on-disk content as
//  literal strings and SHA-256 digests as literal byte arrays, and
//  assert which MachineConfigUpgradeAction comes back. No filesystem,
//  no Win32, no resource extraction.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineConfigUpgradeTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Helpers
    //
    ////////////////////////////////////////////////////////////////////////////

    static string MakeHashHex (char fill)
    {
        // 64-char hex string (32 bytes' worth) — fill is a hex digit.
        return string (64, fill);
    }

    static vector<MachineConfigPriorHash> MakePriors()
    {
        // string_view fields refer back to these static literals so
        // the vector entries' string_views stay valid for the test.
        static const string  s_apple2Hex     = MakeHashHex ('a');
        static const string  s_apple2PlusHex = MakeHashHex ('b');

        vector<MachineConfigPriorHash>  out;

        out.push_back ({ "Apple2",     s_apple2Hex });
        out.push_back ({ "Apple2Plus", s_apple2PlusHex });
        return out;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  NormalizeBytes — BOM strip + CRLF→LF
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (NormalizeBytes_StripsUtf8Bom)
    {
        string  withBom = string ("\xEF\xBB\xBF") + "{ \"name\": \"x\" }";

        Assert::AreEqual (string ("{ \"name\": \"x\" }"),
                          MachineConfigUpgrade::NormalizeBytes (withBom));
    }

    TEST_METHOD (NormalizeBytes_ConvertsCrlfToLf)
    {
        Assert::AreEqual (string ("a\nb\nc"),
                          MachineConfigUpgrade::NormalizeBytes ("a\r\nb\r\nc"));
    }

    TEST_METHOD (NormalizeBytes_LeavesLoneCrAlone)
    {
        // Only CRLF is normalized; lone \r (classic-Mac) stays put.
        Assert::AreEqual (string ("a\rb"),
                          MachineConfigUpgrade::NormalizeBytes ("a\rb"));
    }

    TEST_METHOD (NormalizeBytes_EmptyInput)
    {
        Assert::AreEqual (string (""),
                          MachineConfigUpgrade::NormalizeBytes (""));
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseStamp — $cassoDefault detection
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (ParseStamp_PresentInteger_Returned)
    {
        Assert::AreEqual (3, MachineConfigUpgrade::ParseStamp (
            "{ \"$cassoDefault\": 3, \"name\": \"x\" }"));
    }

    TEST_METHOD (ParseStamp_Missing_ReturnsZero)
    {
        Assert::AreEqual (0, MachineConfigUpgrade::ParseStamp (
            "{ \"name\": \"x\" }"));
    }

    TEST_METHOD (ParseStamp_Unparseable_ReturnsZero)
    {
        Assert::AreEqual (0, MachineConfigUpgrade::ParseStamp ("not json at all"));
    }

    TEST_METHOD (ParseStamp_ParserAcceptsLineComments)
    {
        Assert::AreEqual (5, MachineConfigUpgrade::ParseStamp (
            "// header comment\n{ \"$cassoDefault\": 5 }"));
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — Extract (missing on disk)
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_MissingOnDisk_Extract)
    {
        auto priors = MakePriors();
        auto action = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            nullptr, "",
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::Extract);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — Skip (stamp current or newer)
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_StampEqualsEmbedded_Skip)
    {
        string  content   = "{ \"$cassoDefault\": 2 }";
        string  hashHex   = MakeHashHex ('1');
        auto    priors    = MakePriors();
        auto    action    = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            &content, hashHex,
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::Skip);
    }

    TEST_METHOD (Plan_StampNewerThanEmbedded_Skip)
    {
        string  content   = "{ \"$cassoDefault\": 5 }";
        string  hashHex   = MakeHashHex ('1');
        auto    priors    = MakePriors();
        auto    action    = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            &content, hashHex,
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::Skip);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — OverwriteSilent (stale stamp)
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_StaleStamp_OverwriteSilent)
    {
        string  content = "{ \"$cassoDefault\": 1, \"name\": \"x\" }";
        string  hashHex = MakeHashHex ('1');
        auto    priors  = MakePriors();
        auto    action  = MachineConfigUpgrade::Plan (
            "Apple2", 3,
            &content, hashHex,
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::OverwriteSilent);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — OverwriteSilent (unstamped + known prior hash)
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_UnstampedKnownPrior_OverwriteSilent)
    {
        string  content = "{ \"name\": \"Apple ][\" }";
        string  hashHex = MakeHashHex ('a');
        auto    priors  = MakePriors();
        auto    action  = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            &content, hashHex,
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::OverwriteSilent);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — BackupAndReplace (unstamped + no hash match)
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_UnstampedNoHashMatch_BackupAndReplace)
    {
        string  content = "{ \"name\": \"My custom machine\" }";
        string  hashHex = MakeHashHex ('c');
        auto    priors  = MakePriors();
        auto    action  = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            &content, hashHex,
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::BackupAndReplace);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — hash matches a DIFFERENT machine's prior, not this one
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_HashBelongsToOtherMachine_BackupAndReplace)
    {
        // 'a'*64 is Apple2's prior. Asking about Apple2Plus → no match.
        string  content = "{ \"name\": \"x\" }";
        string  hashHex = MakeHashHex ('a');
        auto    priors  = MakePriors();
        auto    action  = MachineConfigUpgrade::Plan (
            "Apple2Plus", 2,
            &content, hashHex,
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::BackupAndReplace);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — empty hash with present content forces no-match path
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_EmptyHash_BackupAndReplace)
    {
        // Pathological case: caller had content but for some reason
        // didn't precompute a hash. Treated as no match → backup path.
        string  content = "{ \"name\": \"x\" }";
        auto    priors  = MakePriors();
        auto    action  = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            &content, "",
            span<const MachineConfigPriorHash> (priors));

        Assert::IsTrue (action == MachineConfigUpgradeAction::BackupAndReplace);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Plan — empty prior list still works
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Plan_EmptyPriorList_BackupAndReplace)
    {
        string  content = "{ \"name\": \"x\" }";
        string  hashHex = MakeHashHex ('a');
        auto    action  = MachineConfigUpgrade::Plan (
            "Apple2", 2,
            &content, hashHex,
            span<const MachineConfigPriorHash> ());

        Assert::IsTrue (action == MachineConfigUpgradeAction::BackupAndReplace);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  BytesToHex — lowercase hex of 32-byte digest
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BytesToHex_EncodesLowercase)
    {
        array<uint8_t, 4>  in = { 0x00, 0xab, 0xcd, 0xef };

        Assert::AreEqual (string ("00abcdef"),
                          MachineConfigUpgrade::BytesToHex (in));
    }

    TEST_METHOD (BytesToHex_FullDigestIs64Chars)
    {
        array<uint8_t, 32>  in = {};

        Assert::AreEqual (size_t (64),
                          MachineConfigUpgrade::BytesToHex (in).size());
    }
};
