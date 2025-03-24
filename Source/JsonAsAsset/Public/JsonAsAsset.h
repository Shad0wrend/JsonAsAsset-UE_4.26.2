/* Copyright JsonAsAsset Contributors 2024-2025 */

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Utilities/Serializers/PropertyUtilities.h"
#include "Utilities/AppStyleCompatibility.h"

#if ENGINE_UE4
#include "Modules/ModuleInterface.h"
#endif

class UJsonAsAssetSettings;

struct FJsonAsAssetVersioning {
public:
    bool bNewVersionAvailable = false;
    bool bFutureVersion = false;
    bool bLatestVersion = false;
    
    FJsonAsAssetVersioning() = default;
    
    FJsonAsAssetVersioning(const int Version, const int LatestVersion, const FString& InHTMLUrl, const FString& VersionName, const FString& CurrentVersionName)
        : Version(Version)
        , LatestVersion(LatestVersion)
        , VersionName(VersionName)
        , CurrentVersionName(CurrentVersionName)
        , HTMLUrl(InHTMLUrl)
    {
        bNewVersionAvailable = LatestVersion > Version;
        bFutureVersion = Version > LatestVersion;
        
        bLatestVersion = !(bNewVersionAvailable || bFutureVersion);
    }

    int Version = 0;
    int LatestVersion = 0;

    FString VersionName = "";
    FString CurrentVersionName = "";

    FString HTMLUrl = "";

    bool bIsValid = false;

    void SetValid(const bool bValid) {
        bIsValid = bValid;
    }
};

class FJsonAsAssetModule : public IModuleInterface {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /* Execute File Dialog */
    void PluginButtonClicked();

    FJsonAsAssetVersioning Versioning;

    void CheckForUpdates();

private:
    UPROPERTY()
    UPropertySerializer* PropertySerializer;

    UPROPERTY()
    UObjectSerializer* GObjectSerializer;
    
    void RegisterMenus();

    TSharedPtr<FUICommandList> PluginCommands;
    TSharedRef<SWidget> CreateToolbarDropdown();
    void CreateLocalFetchDropdown(FMenuBuilder MenuBuilder) const;
    void CreateVersioningDropdown(FMenuBuilder MenuBuilder) const;
    void CreateLastDropdown(FMenuBuilder MenuBuilder) const;

    static void SupportedAssetsDropdown(FMenuBuilder& InnerMenuBuilder, bool bIsLocalFetch = false);

    bool bActionRequired = false;
    UJsonAsAssetSettings* Settings = nullptr;

    TSharedPtr<IPlugin> Plugin;

#if ENGINE_UE4
    void AddToolbarExtension(FToolBarBuilder& Builder);
#endif
};