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


    // Parse the "$cassoDefault" integer field out of `content`.
    // Returns 0 if the file is unparseable or the field is missing
    // — i.e. the pre-versioning era.
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
};
