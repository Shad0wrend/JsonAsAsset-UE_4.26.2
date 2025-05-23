/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Settings/Details/JsonAsAssetSettingsDetails.h"
#include "Settings/JsonAsAssetSettings.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Utilities/RemoteUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Modules/LocalFetchModule.h"
#include "Utilities/EngineUtilities.h"

#if ENGINE_UE4
#include "DetailCategoryBuilder.h"
#endif

#define LOCTEXT_NAMESPACE "JsonAsAssetSettingsDetails"

TSharedRef<IDetailCustomization> FJsonAsAssetSettingsDetails::MakeInstance() {
	return MakeShareable(new FJsonAsAssetSettingsDetails);
}

void FJsonAsAssetSettingsDetails::InitializeAESKeys(UJsonAsAssetSettings* PluginSettings) {
	const TSharedPtr<FJsonObject> AES = RequestObjectURL("https://fortnitecentral.genxgames.gg/api/v1/aes");

	if (!AES.IsValid()) return;
	
	PluginSettings->DynamicKeys.Empty();
	PluginSettings->ArchiveKey = AES->GetStringField(TEXT("mainKey"));
	
	for (const TSharedPtr<FJsonValue> Value : AES->GetArrayField(TEXT("dynamicKeys"))) {
		const TSharedPtr<FJsonObject> Object = Value->AsObject();

		const FString GUID = Object->GetStringField(TEXT("guid"));
		const FString Key = Object->GetStringField(TEXT("key"));

		PluginSettings->DynamicKeys.Add(FLocalFetchAES(GUID, Key));
	}

	SavePluginConfig(PluginSettings);
}

void FJsonAsAssetSettingsDetails::InitializeMappings(const UJsonAsAssetSettings* PluginSettings) {
	FString LocalExportDirectory = PluginSettings->ExportDirectory.Path;

	if (LocalExportDirectory != "" && LocalExportDirectory.Contains("/")) {
		FString DataFolder; {
			if (LocalExportDirectory.EndsWith("/")) {
				LocalExportDirectory.Split("/", &DataFolder, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			} else {
				DataFolder = LocalExportDirectory;
			}

			DataFolder.Split("/", &DataFolder, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd); {
				DataFolder = DataFolder + "/.data";
			}
		}

		const TArray<TSharedPtr<FJsonValue>> MappingsArray = RequestArrayURL("https://fortnitecentral.genxgames.gg/api/v1/mappings");
		
		TSharedPtr<FJsonValue> MappingsValue; {
			if (MappingsArray.IsValidIndex(1)) {
				MappingsValue = MappingsArray[1];
			} else {
				MappingsValue = MappingsArray[0];
			}
		}

		if (MappingsValue == nullptr) return;
		
		const TSharedPtr<FJsonObject> MappingsObject = MappingsValue->AsObject();

		FString FileName = MappingsObject->GetStringField(TEXT("fileName"));

		auto OnRequestComplete = [DataFolder, FileName](FHttpRequestPtr, const FHttpResponsePtr& Response, const bool bWasSuccessful) {
			if (bWasSuccessful) {
				UJsonAsAssetSettings* ReferenceSettings = GetMutableDefault<UJsonAsAssetSettings>();

				FFileHelper::SaveArrayToFile(Response->GetContent(), *(DataFolder + "/" + FileName));
				ReferenceSettings->MappingFilePath.FilePath = DataFolder + "/" + FileName;
				
				SavePluginConfig(ReferenceSettings);
			}
		};

		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MappingsRequest = FHttpModule::Get().CreateRequest();
		MappingsRequest->SetVerb("GET");
		
		const FString URL = MappingsObject->GetStringField(TEXT("url"));
		MappingsRequest->SetURL(URL);
		
		MappingsRequest->OnProcessRequestComplete().BindLambda(OnRequestComplete);
		MappingsRequest->ProcessRequest();
	}
}

void FJsonAsAssetSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	/* Reference to settings */
	const TWeakObjectPtr<UJsonAsAssetSettings> Settings = Cast<UJsonAsAssetSettings>(ObjectsBeingCustomized[0].Get());

	EditConfiguration(DetailBuilder);

	DetailBuilder.EditCategory("Local Fetch", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Local Fetch - Configuration", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Local Fetch - Encryption", FText::GetEmpty(), ECategoryPriority::Important);

	EditEncryption(Settings, DetailBuilder);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void FJsonAsAssetSettingsDetails::EditConfiguration(IDetailLayoutBuilder& DetailBuilder) {
	IDetailCategoryBuilder& AssetCategory = DetailBuilder.EditCategory("Configuration", FText::GetEmpty(), ECategoryPriority::Important);
	
	AssetCategory.AddCustomRow(LOCTEXT("UseFModelAppSettings", "UseFModelAppSettings"))
    .NameContent()
    [
        /* This defines the name/title of the row */
        SNew(STextBlock)
        .Text(LOCTEXT("UseFModelAppSettings", "Load External Configuration"))
        .Font(IDetailLayoutBuilder::GetDetailFont())
    ]
    .ValueContent()
    [
        /* Now we define the value/content of the row */
        SNew(SButton)
        .Text(LOCTEXT("UseFModelAppSettings_Text", "FModel Settings"))
    	.ToolTipText(LOCTEXT("UseFModelAppSettings_Tooltip", "Imports settings from AppData/Roaming/FModel/AppSettings.json"))
        .OnClicked_Lambda([this]()
        {
            UJsonAsAssetSettings* PluginSettings = GetMutableDefault<UJsonAsAssetSettings>();

            /* Get the path to AppData\Roaming */
            FString AppDataPath = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
            AppDataPath = FPaths::Combine(AppDataPath, TEXT("FModel/AppSettings.json"));

        	FString JsonContent;
        	
            if (FFileHelper::LoadFileToString(JsonContent, *AppDataPath)) {
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
            	TSharedPtr<FJsonObject> JsonObject;

                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) {
                    /* Load the PropertiesDirectory and GameDirectory */
                    PluginSettings->ExportDirectory.Path = JsonObject->GetStringField(TEXT("PropertiesDirectory")).Replace(TEXT("\\"), TEXT("/"));
                    PluginSettings->ArchiveDirectory.Path = JsonObject->GetStringField(TEXT("GameDirectory")).Replace(TEXT("\\"), TEXT("/"));

					const FString GameDirectory = JsonObject->GetStringField(TEXT("GameDirectory"));

                    /* Handling AES Keys */
                    if (JsonObject->HasField(TEXT("PerDirectory"))) {
                        const TSharedPtr<FJsonObject> PerDirectoryObject = JsonObject->GetObjectField(TEXT("PerDirectory"));

                        if (PerDirectoryObject->HasField(GameDirectory)) {
                            const TSharedPtr<FJsonObject> PakSettings = PerDirectoryObject->GetObjectField(GameDirectory);

                            if (PakSettings->HasField(TEXT("AesKeys"))) {
                                const TSharedPtr<FJsonObject> AesKeysObject = PakSettings->GetObjectField(TEXT("AesKeys"));

                                if (AesKeysObject->HasField(TEXT("mainKey"))) {
                                    PluginSettings->ArchiveKey = AesKeysObject->GetStringField(TEXT("mainKey"));
                                }

                                if (AesKeysObject->HasField(TEXT("dynamicKeys"))) {
                                    const TArray<TSharedPtr<FJsonValue>> DynamicKeysArray = AesKeysObject->GetArrayField(TEXT("dynamicKeys"));
                                    PluginSettings->DynamicKeys.Empty();

                                    for (const auto& KeyValue : DynamicKeysArray) {
                                        const TSharedPtr<FJsonObject> KeyObject = KeyValue->AsObject();

                                        if (KeyObject.IsValid()) {
                                            FLocalFetchAES NewKey;
                                        	
                                            NewKey.Guid = KeyObject->GetStringField(TEXT("guid"));
                                            NewKey.Value = KeyObject->GetStringField(TEXT("key"));
                                        	
                                            PluginSettings->DynamicKeys.Add(NewKey);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

        	SavePluginConfig(PluginSettings);

            return FReply::Handled();
        })
    ];
}

// ReSharper disable once CppMemberFunctionMayBeConst
void FJsonAsAssetSettingsDetails::EditEncryption(TWeakObjectPtr<UJsonAsAssetSettings> Settings, IDetailLayoutBuilder& DetailBuilder) {
	IDetailCategoryBuilder& EncryptionCategory = DetailBuilder.EditCategory("Local Fetch - Encryption", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Local Fetch - Director", FText::GetEmpty(), ECategoryPriority::Important);

	EncryptionCategory.AddCustomRow(LOCTEXT("EncryptionKeyFetcher", "EncryptionKeyFetcher"))
		.NameContent()
		[
			/* This defines the name/title of the row */
			SNew(STextBlock)
				.Text(LOCTEXT("UseFModelAppSettings", "Fetch Encryption from a API"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()[
		SNew(SButton)
		.Text(LOCTEXT("FetchEncryptionKey", "Fortnite Central API"))
		.ToolTipText(LOCTEXT("FetchEncryptionKey_Tooltip", "Retrieves encryption keys from the Fortnite Central API"))
		.OnClicked_Lambda([this]
		{
			UJsonAsAssetSettings* PluginSettings = GetMutableDefault<UJsonAsAssetSettings>();

			InitializeAESKeys(PluginSettings);
			InitializeMappings(PluginSettings);

			return FReply::Handled();
		}).IsEnabled_Lambda([this, Settings]()
		{
			return Settings->bEnableLocalFetch;
		})
	];
}

#undef LOCTEXT_NAMESPACE
