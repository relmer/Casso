#include "Pch.h"

#include "Update/Sha256Digest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Sha256Digest::Compute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sha256Digest::Compute (std::span<const Byte> bytes, std::vector<Byte> & outDigest)
{
    HRESULT             hr     = S_OK;
    BCRYPT_ALG_HANDLE   hAlg   = nullptr;
    BCRYPT_HASH_HANDLE  hHash  = nullptr;
    NTSTATUS            status = 0;



    outDigest.assign (kDigestBytes, 0);

    status = BCryptOpenAlgorithmProvider (&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    CBRA (BCRYPT_SUCCESS (status));

    status = BCryptCreateHash (hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    CBRA (BCRYPT_SUCCESS (status));

    status = BCryptHashData (hHash, const_cast<PUCHAR> (bytes.data()), static_cast<ULONG> (bytes.size()), 0);
    CBRA (BCRYPT_SUCCESS (status));

    status = BCryptFinishHash (hHash, outDigest.data(), static_cast<ULONG> (outDigest.size()), 0);
    CBRA (BCRYPT_SUCCESS (status));

Error:
    if (hHash != nullptr)
    {
        BCryptDestroyHash (hHash);
    }

    if (hAlg != nullptr)
    {
        BCryptCloseAlgorithmProvider (hAlg, 0);
    }

    if (FAILED (hr))
    {
        outDigest.clear();
    }

    return hr;
}
