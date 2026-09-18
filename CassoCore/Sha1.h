#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1
//
//  SHA-1 as RFC 3174 defines it, over bytes in memory. A debug file records the
//  hash of each source file's text, and the debugger hashes a candidate file
//  to tell whether it is the one the program was built from.
//
//  THE TEXT HASH NORMALIZES LINE ENDINGS FIRST. CR LF and a lone CR both become
//  LF, so a file checked out with Windows line endings and the same file with
//  Unix ones hash alike; a git checkout or an editor changing them is not an
//  edit.
//
//  For identifying files, not for security.
//
////////////////////////////////////////////////////////////////////////////////

class Sha1
{
public:
    static constexpr size_t  kDigestBytes = 20;

    using Digest = std::array<uint8_t, kDigestBytes>;

    static Digest       Compute              (std::span<const uint8_t> bytes);
    static std::string  ToHex                (const Digest & digest);
    static std::string  NormalizeLineEndings (std::string_view text);

    //  Forty lowercase hex digits for text with its line endings normalized,
    //  which is the form a debug file records.
    static std::string  ComputeTextHex       (std::string_view text);

private:
    static constexpr size_t  kBlockBytes = 64;

    using State = std::array<uint32_t, 5>;

    static void      ProcessBlock (const uint8_t * block, State & state);
    static uint32_t  RotateLeft   (uint32_t value, int bits);
};
