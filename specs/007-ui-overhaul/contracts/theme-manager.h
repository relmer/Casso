// Contract: Casso/Ui/ThemeManager.h
//
// This file documents the public surface of ThemeManager for the native
// UI pipeline.

#pragma once

namespace Casso::Ui
{
    struct ThemeInfo
    {
        std::string  name;
        std::string  familyId;
        std::string  variantId;
        std::string  author;
        std::string  description;
        int          version;
        std::string  directoryPath;
        bool         useMicaBackdrop;
    };

    enum class ThemeLoadResult
    {
        Success,
        MetadataMissing,
        MetadataInvalid,
        TokensInvalid,
        AssetMissing,
        VersionTooNew,
        UnknownError
    };


    class ThemeManager
    {
    public:
        ThemeManager (Config::IFileSystem & fs);

        HRESULT Discover ();
        const std::vector<ThemeInfo> & GetAvailableThemes () const;
        HRESULT ActivateByName (const std::string & themeName);
        HRESULT ActivateByFamilyVariant (const std::string & familyId,
                                         const std::string & variantId);
        HRESULT ReloadCurrent ();
        const std::string & GetActiveThemeName () const;
        const std::string & GetActiveFamilyId () const;
        const std::string & GetActiveVariantId () const;

        using ChangeListener = std::function<void (const ThemeInfo &)>;
        void  AddChangeListener (ChangeListener listener);

    private:
        // ... implementation details ...
    };
}
