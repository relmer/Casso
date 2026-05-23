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


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseStamp — 007 : new key + legacy fallback
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (ParseStamp_NewKeyPresent_Returned)
    {
        Assert::AreEqual (7, MachineConfigUpgrade::ParseStamp (
            "{ \"$cassoMachineVersion\": 7, \"name\": \"x\" }"));
    }

    TEST_METHOD (ParseStamp_NewKeyTakesPrecedenceOverLegacy)
    {
        // Forward-compat: if both keys are present (e.g. a partially
        // migrated user file), the new key wins.
        Assert::AreEqual (9, MachineConfigUpgrade::ParseStamp (
            "{ \"$cassoMachineVersion\": 9, \"$cassoDefault\": 4 }"));
    }

    TEST_METHOD (ParseStamp_NeitherKey_ReturnsZero)
    {
        Assert::AreEqual (0, MachineConfigUpgrade::ParseStamp (
            "{ \"name\": \"x\" }"));
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MigrateUserConfig — 007 : rename legacy key
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (MigrateUserConfig_RenamesLegacyKey)
    {
        string   input    = "{ \"$cassoDefault\": 2, \"name\": \"x\" }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_OK,
            L"Migration of legacy key should report S_OK (changes applied).");
        Assert::IsTrue (migrated.find ("\"$cassoMachineVersion\"") != string::npos,
            L"Migrated output must contain the new key name.");
        Assert::IsTrue (migrated.find ("\"$cassoDefault\"") == string::npos,
            L"Migrated output must no longer contain the legacy key name.");
        Assert::IsTrue (migrated.find ("\"name\": \"x\"") != string::npos,
            L"All other fields must be preserved verbatim.");
    }

    TEST_METHOD (MigrateUserConfig_Idempotent)
    {
        string   input    = "{\n    \"$cassoDefault\": 2,\n    \"name\": \"x\"\n}";
        string   first;
        string   second;
        HRESULT  hrFirst  = MachineConfigUpgrade::MigrateUserConfig (input, first);
        HRESULT  hrSecond = MachineConfigUpgrade::MigrateUserConfig (first, second);

        Assert::IsTrue (hrFirst  == S_OK,
            L"First pass must report changes applied.");
        Assert::IsTrue (hrSecond == S_FALSE,
            L"Second pass must report no-op (S_FALSE).");
        Assert::AreEqual (first, second,
            L"Second pass output must be byte-for-byte identical to first.");
    }

    TEST_METHOD (MigrateUserConfig_AlreadyNewSchema_NoOp)
    {
        string   input    = "{ \"$cassoMachineVersion\": 3, \"name\": \"x\" }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_FALSE,
            L"Already-migrated content must report S_FALSE.");
        Assert::AreEqual (input, migrated,
            L"Already-migrated content must be returned unchanged.");
    }

    TEST_METHOD (MigrateUserConfig_BothVersionFields_RenamesLegacyAlias)
    {
        string   input    = "{ \"$cassoMachineVersion\": 9, \"$cassoDefault\": 4, \"name\": \"x\" }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_OK,
            L"Presence of legacy alias should trigger rewrite.");
        Assert::IsTrue (migrated.find ("\"$cassoDefault\"") == string::npos,
            L"Legacy alias must be removed from migrated output.");
        Assert::IsTrue (migrated.find ("\"$cassoMachineVersion\"") != string::npos,
            L"Migrated output must keep canonical key.");
    }

    TEST_METHOD (MigrateUserConfig_LegacyKeyAsStringValue_NotRewritten)
    {
        // Edge case: the literal "$cassoDefault" appearing as a string
        // VALUE (not a key) must not be rewritten. We confirm this by
        // checking that a content with the token followed by a comma
        // (rather than a colon) is left alone.
        string   input    = "{ \"description\": \"$cassoDefault\", \"v\": 1 }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_FALSE,
            L"Token used as a value (not a key) must not trigger migration.");
        Assert::AreEqual (input, migrated,
            L"Content must be returned unchanged.");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MigrateUserConfig — capabilityFlag default injection
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (MigrateUserConfig_InternalDevicesDefaultRequired)
    {
        string   input    = "{ \"$cassoMachineVersion\": 3,"
                            "  \"internalDevices\": ["
                            "    { \"type\": \"keyboard\" },"
                            "    { \"type\": \"speaker\", \"capabilityFlag\": \"optional\" }"
                            "  ] }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_OK,
            L"Missing capabilityFlag in internalDevices must trigger migration.");
        Assert::IsTrue (migrated.find ("\"capabilityFlag\": \"required\"") != string::npos,
            L"Entry without an explicit flag must default to 'required'.");
        Assert::IsTrue (migrated.find ("\"capabilityFlag\": \"optional\"") != string::npos,
            L"Pre-existing 'optional' flag must be preserved.");
    }


    TEST_METHOD (MigrateUserConfig_SlotsDefaultOptional)
    {
        string   input    = "{ \"$cassoMachineVersion\": 3,"
                            "  \"slots\": ["
                            "    { \"slot\": 1 },"
                            "    { \"slot\": 6, \"capabilityFlag\": \"platform-locked\" }"
                            "  ] }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_OK);
        Assert::IsTrue (migrated.find ("\"capabilityFlag\": \"optional\"") != string::npos,
            L"slots[] entries without a flag must default to 'optional'.");
        Assert::IsTrue (migrated.find ("\"capabilityFlag\": \"platform-locked\"") != string::npos,
            L"Pre-existing slot lock-reason must be preserved.");
    }


    TEST_METHOD (MigrateUserConfig_BothKeysAndMissingFlags_AppliesAllChanges)
    {
        string   input    = "{ \"$cassoMachineVersion\": 3,"
                            "  \"$cassoDefault\": 2,"
                            "  \"internalDevices\": [ { \"type\": \"k\" } ],"
                            "  \"slots\": [ { \"slot\": 1 } ] }";
        string   migrated;
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == S_OK);
        Assert::IsTrue (migrated.find ("\"$cassoDefault\"")        == string::npos,
            L"Legacy alias must be dropped when canonical is present.");
        Assert::IsTrue (migrated.find ("\"$cassoMachineVersion\"") != string::npos);
        Assert::IsTrue (migrated.find ("\"capabilityFlag\": \"required\"") != string::npos);
        Assert::IsTrue (migrated.find ("\"capabilityFlag\": \"optional\"") != string::npos);
    }


    TEST_METHOD (MigrateUserConfig_CapabilityFlag_Idempotent)
    {
        string   input    = "{ \"$cassoMachineVersion\": 3,"
                            "  \"internalDevices\": [ { \"type\": \"k\" } ] }";
        string   first;
        string   second;
        HRESULT  hr1      = MachineConfigUpgrade::MigrateUserConfig (input, first);
        HRESULT  hr2      = MachineConfigUpgrade::MigrateUserConfig (first, second);

        Assert::IsTrue (hr1 == S_OK,
            L"First pass injects the missing flag.");
        Assert::IsTrue (hr2 == S_FALSE,
            L"Second pass must report no-op once flag is present.");
        Assert::AreEqual (first, second,
            L"Second-pass output must be byte-for-byte identical to first.");
    }


    TEST_METHOD (MigrateUserConfig_UnparseableInput_E_INVALIDARG)
    {
        string   input    = "{\"unterminated\":";
        string   migrated = "leftover sentinel";
        HRESULT  hr       = MachineConfigUpgrade::MigrateUserConfig (input, migrated);

        Assert::IsTrue (hr == E_INVALIDARG,
            L"Unparseable input must be reported as E_INVALIDARG.");
        Assert::IsTrue (migrated.empty(),
            L"On failure outMigrated must be cleared.");
    }
};
