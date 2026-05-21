#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigUpgradeAction
//
//  The decision an upgrader makes for a single on-disk embedded
//  machine config when EnsureMachineConfigs runs at startup.
//
////////////////////////////////////////////////////////////////////////////////

enum class MachineConfigUpgradeAction
{
    Skip,              // file is up to date or unreadable — leave alone
    Extract,           // file is missing — write the embedded default
    OverwriteSilent,   // safe to replace (stale stamp or known prior hash)
    BackupAndReplace,  // presumed user-edited — rename aside, then write
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigPriorHash
//
//  One historical embedded-default fingerprint. `hashHex` is the
//  lowercase-hex SHA-256 (64 chars) of the file's content after BOM
//  strip + CRLF → LF normalization. The list lives in production
//  code (AssetBootstrap.cpp) and gets one new entry every time we
//  bump an embedded config's version.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineConfigPriorHash
{
    string_view  machineName;
    string_view  hashHex;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigUpgrade
//
//  Pure decision module for embedded-machine-config upgrades. All
//  three operations are side-effect-free and testable without any
//  Win32 surface — the production wrapper in AssetBootstrap supplies
//  the on-disk content (or null when missing) and a precomputed
//  SHA-256, and acts on the returned MachineConfigUpgradeAction.
//
////////////////////////////////////////////////////////////////////////////////

class MachineConfigUpgrade
{
public:

    // Strip a UTF-8 BOM and collapse CRLF to LF so the SHA-256 of a
    // config doesn't depend on which editor (or which build host)
    // last touched it.
    static string NormalizeBytes (const string & content);


    // Parse the "$cassoMachineVersion" integer field out of `content`,
    // falling back to the legacy "$cassoDefault" key if the new key is
    // missing. Returns 0 if the file is unparseable or neither field
    // is present — i.e. the pre-versioning era.
    static int ParseStamp (const string & content);


    // Lowercase-hex encode 32 bytes of digest into a 64-char string.
    static string BytesToHex (span<const uint8_t> bytes);


    // The decision. `diskContent == nullptr` means the file is
    // missing on disk; `diskNormalizedHashHex` empty means the
    // caller didn't precompute a hash (only valid alongside
    // diskContent == nullptr). `priorHashes` is the table of
    // historical embedded-default digests for any machine.
    static MachineConfigUpgradeAction Plan (
        string_view                                machineName,
        int                                        embeddedVersion,
        const string                             * diskContent,
        string_view                                diskNormalizedHashHex,
        span<const MachineConfigPriorHash>         priorHashes);


    // Migrate a user-authored `<Machine>_user.json` content forward
    // To the current schema (007-ui-overhaul, ):
    //
    //   1. Renames JSON key `$cassoDefault` to `$cassoMachineVersion`.
    //   2. Inserts a default `"capabilityFlag"` field on every
    //      internalDevices[] entry that lacks one (default: "required").
    //   3. Inserts a default `"capabilityFlag"` field on every
    //      slots[] entry that lacks one (default: "optional").
    //
    // The operation is idempotent: running it twice produces the same
    // output the second time as the first. Content that is already at
    // the new schema is returned unchanged. Operates as a textual
    // transform over the source bytes — comments, whitespace, and key
    // ordering are preserved everywhere outside the rewritten regions.
    // Returns S_FALSE if no migration was needed, S_OK otherwise; any
    // failure (malformed JSON detected by the inserter) leaves
    // outMigrated empty and returns E_INVALIDARG.
    static HRESULT MigrateUserConfig (
        const string & content,
        string       & outMigrated);
};
