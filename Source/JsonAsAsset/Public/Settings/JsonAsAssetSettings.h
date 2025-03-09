/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Modules/LocalFetchModule.h"
#include "JsonAsAssetSettings.generated.h"

/* Settings for materials */
USTRUCT()
struct FJMaterialImportSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FJMaterialImportSettings()
		: bSkipResultNodeConnection(false)
	{}

	/**
	 * Prevents a known error during Material asset import/download:
	 * "Material expression called Compiler->TextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly."
	 *
	 * To avoid this issue, this option skips connecting the inputs to the material's primary result node, potentially fixing the error.
	 *
	 * Usage:
	 *  - If enabled, import the material, save your project, restart the editor, and then re-import the material.
	 *  - Alternatively, manually connect the inputs to the main result node.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Material Import Settings")
	bool bSkipResultNodeConnection;
};

/* Settings for textures */
USTRUCT()
struct FJTextureImportSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FJTextureImportSettings()
		: bDownloadExistingTextures(false)
	{}

	/**
	 * Enables re-downloading of textures even if they already exist in the Unreal Engine project.
	 *
	 * Use Case:
	 * This option is useful when you need to re-import textures that have been updated in a newer version of your Local Fetch build.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch - Encryption", AdvancedDisplay)
	bool bDownloadExistingTextures;
};

/* Settings for sounds */
USTRUCT()
struct FJSoundImportSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Config, Category = "Sound Import Settings", meta = (DisplayName = "Audio File Name Extension"))
	FString AudioFileExtension = "ogg";
};

USTRUCT()
struct FAssetSettings
{
	GENERATED_BODY()
public:
	/* Constructor to initialize default values */
	FAssetSettings()
		: bSavePackagesOnImport(false), bEnableAssetTools(false)
	{
		MaterialImportSettings = FJMaterialImportSettings();
		SoundImportSettings = FJSoundImportSettings();
		TextureImportSettings = FJTextureImportSettings();
	}

	UPROPERTY(EditAnywhere, Config, Category = "Asset Settings")
	FJTextureImportSettings TextureImportSettings;
	
	UPROPERTY(EditAnywhere, Config, Category = "Asset Settings")
	FJMaterialImportSettings MaterialImportSettings;

	UPROPERTY(EditAnywhere, Config, Category = "Asset Settings")
	FJSoundImportSettings SoundImportSettings;

	UPROPERTY(EditAnywhere, Config, Category = "Asset Settings", meta = (DisplayName = "Save Assets On Import"))
	bool bSavePackagesOnImport;

	UPROPERTY(EditAnywhere, Config, Category = "Asset Settings", meta = (DisplayName = "Enable Asset Tools (experimental)"))
	bool bEnableAssetTools;

	/**
	* This property is generally not required for normal operations, but is necessary for compatibility with older versions of game builds.
	*/
	UPROPERTY(EditAnywhere, Config, Category = "Asset Settings")
	FString GameName;
};

/* A user-friendly Unreal Engine plugin designed to import assets from packaged games through JSON files */
UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig)
class JSONASASSET_API UJsonAsAssetSettings : public UDeveloperSettings {
	GENERATED_BODY()

public:
	UJsonAsAssetSettings();

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif

	/**
	 * Specifies the directory path for exported assets.
	 * (e.g., Output/Exports)
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Configuration")
	FDirectoryPath ExportDirectory;
	
	UPROPERTY(EditAnywhere, Config, Category = "Configuration")
	FAssetSettings AssetSettings;

	/**
	 * Retrieves assets from a locally hosted API and imports them directly into your project.
	 *
	 * Before initiating the Local Fetch, ensure that all configuration settings are properly set.
	 * For further instructions, please refer to the README.md file found on GitHub.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch")
	bool bEnableLocalFetch;

	/**
	 * Specific Paks folder of your game
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch - Configuration", meta=(EditCondition="bEnableLocalFetch", DisplayName="Directory"))
	FDirectoryPath ArchiveDirectory;

	/**
	 * Unreal Engine version of your game
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch - Configuration", meta=(EditCondition="bEnableLocalFetch", DisplayName="Unreal Engine"))
	TEnumAsByte<ECUE4ParseVersion> UnrealVersion;

	/**
	 * Specifies the file path to your mappings file
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch - Configuration", meta=(EditCondition="bEnableLocalFetch", FilePathFilter="usmap", RelativeToGameDir, DisplayName="Mappings File .usmap"))
	FFilePath MappingFilePath;

	/**
	 * Specifies the main encryption key used for decrypting encrypted game files.
	 *
	 * Default value:
	 *   AES Key: 0x00000000000000000000000000000000000000000000000000000000000
	 *
	 * Note: This key is optional for most Unreal Engine games. Override it only if your build uses encryption.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch - Encryption", meta=(EditCondition="bEnableLocalFetch", DisplayName="Encryption Key"))
	FString ArchiveKey = "0x00000000000000000000000000000000000000000000000000000000000";

	/**
	 * Specifies additional encryption keys used for decrypting encrypted game files.
	 *
	 * Note: These keys are optional for most Unreal Engine games. Override them only if your build uses encryption.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch - Encryption", meta=(EditCondition="bEnableLocalFetch", DisplayName="Dynamic Keys"))
	TArray<FAesKey> DynamicKeys;
	/**
	 * Local Fetch URL
	 *
	 * Default: "http://localhost:1500"
	 * Note: This setting is intended for advanced users. It is recommended to leave it unchanged unless you are certain of your requirements.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Local Fetch", meta=(EditCondition="bEnableLocalFetch", DisplayName = "Local Fetch URL"), AdvancedDisplay)
	FString LocalFetchUrl = "http://localhost:1500";
};
