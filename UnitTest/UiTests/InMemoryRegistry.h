#pragma once

#include "Pch.h"

#include "Config/IRegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InMemoryRegistry
//
//  Test-only `IRegistrySettings` implementation backed by `std::map`.
//  Values are keyed by the (subkey, valueName) pair so two distinct
//  subkeys (e.g. monitor topology hashes) round-trip independently.
//
//  Threading: tests in this suite are single-threaded; no mutex is
//  needed.
//
//  Header-only on purpose — lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class InMemoryRegistry : public IRegistrySettings
{
public:
    HRESULT ReadString (
        LPCWSTR              subkey,
        LPCWSTR              valueName,
        std::wstring       & outValue) override
    {
        auto  it = m_strings.find (Key (subkey, valueName));

        if (it == m_strings.end())
        {
            return S_FALSE;
        }

        outValue = it->second;
        return S_OK;
    }


    HRESULT WriteString (
        LPCWSTR              subkey,
        LPCWSTR              valueName,
        const std::wstring & value) override
    {
        m_strings[Key (subkey, valueName)] = value;
        return S_OK;
    }


    HRESULT ReadDword (
        LPCWSTR              subkey,
        LPCWSTR              valueName,
        DWORD              & outValue) override
    {
        auto  it = m_dwords.find (Key (subkey, valueName));

        if (it == m_dwords.end())
        {
            return S_FALSE;
        }

        outValue = it->second;
        return S_OK;
    }


    HRESULT WriteDword (
        LPCWSTR              subkey,
        LPCWSTR              valueName,
        DWORD                value) override
    {
        m_dwords[Key (subkey, valueName)] = value;
        return S_OK;
    }


    // Test introspection helpers.
    size_t  StringCount () const { return m_strings.size(); }
    size_t  DwordCount  () const { return m_dwords.size();  }

    bool    HasString (LPCWSTR subkey, LPCWSTR valueName) const
    {
        return m_strings.find (Key (subkey, valueName)) != m_strings.end();
    }

    std::wstring  GetString (LPCWSTR subkey, LPCWSTR valueName) const
    {
        auto  it = m_strings.find (Key (subkey, valueName));
        return it == m_strings.end() ? std::wstring() : it->second;
    }


private:
    static std::wstring Key (LPCWSTR subkey, LPCWSTR valueName)
    {
        std::wstring  s = subkey == nullptr ? std::wstring() : std::wstring (subkey);

        s += L"\x1f";   // unit separator -- can't appear in a real value name
        s += valueName == nullptr ? std::wstring() : std::wstring (valueName);
        return s;
    }

    std::map<std::wstring, std::wstring>  m_strings;
    std::map<std::wstring, DWORD>         m_dwords;
};
