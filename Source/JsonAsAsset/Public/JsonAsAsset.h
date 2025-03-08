/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "CoreMinimal.h"
#include "Utilities/Serializers/PropertyUtilities.h"

#if ENGINE_MAJOR_VERSION == 4
#include "Modules/ModuleInterface.h"
#endif

class UJsonAsAssetSettings;

class FJsonAsAssetModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /* Execute File Dialog */
    void PluginButtonClicked();

private:
    UPROPERTY()
    UPropertySerializer* PropertySerializer;

    UPROPERTY()
    UObjectSerializer* GObjectSerializer;
    
    void RegisterMenus();

    TSharedPtr<FUICommandList> PluginCommands;
    TSharedRef<SWidget> CreateToolbarDropdown();
    void CreateLocalFetchDropdown(FMenuBuilder MenuBuilder) const;
    void ImportConvexCollision() const;

    bool bActionRequired = false;
    UJsonAsAssetSettings* Settings = nullptr;

#if ENGINE_MAJOR_VERSION == 4
    void AddToolbarExtension(FToolBarBuilder& Builder);
#endif
};