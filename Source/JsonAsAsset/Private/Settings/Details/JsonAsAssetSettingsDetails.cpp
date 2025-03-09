/* Copyright JAA Contributors 2024-2025 */

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

#if ENGINE_MAJOR_VERSION == 4
#include "DetailCategoryBuilder.h"
#endif

#define LOCTEXT_NAMESPACE "JsonAsAssetSettingsDetails"

TSharedRef<IDetailCustomization> FJsonAsAssetSettingsDetails::MakeInstance() {
	return MakeShareable(new FJsonAsAssetSettingsDetails);
}

void FJsonAsAssetSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) {
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	/* Reference to settings */
	TWeakObjectPtr<UJsonAsAssetSettings> Settings = Cast<UJsonAsAssetSettings>(ObjectsBeingCustomized[0].Get());

	EditConfiguration(Settings, DetailBuilder);

	DetailBuilder.EditCategory("Local Fetch", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Local Fetch - Configuration", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("Local Fetch - Encryption", FText::GetEmpty(), ECategoryPriority::Important);

	EditEncryption(Settings, DetailBuilder);
}

void FJsonAsAssetSettingsDetails::EditConfiguration(TWeakObjectPtr<UJsonAsAssetSettings> Settings, IDetailLayoutBuilder& DetailBuilder) {
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
        .OnClicked_Lambda([this, Settings]()
        {
            UJsonAsAssetSettings* PluginSettings = GetMutableDefault<UJsonAsAssetSettings>();

            /* Get the path to AppData\Roaming */
            FString AppDataPath = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
            AppDataPath = FPaths::Combine(AppDataPath, TEXT("FModel/AppSettings.json"));

        	FString JsonContent;
        	
            if (FFileHelper::LoadFileToString(JsonContent, *AppDataPath)) {
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
            	TSharedPtr<FJsonObject> JsonObject;

                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid()) {
                    /* Load the PropertiesDirectory and GameDirectory */
                    PluginSettings->ExportDirectory.Path = JsonObject->GetStringField(TEXT("PropertiesDirectory")).Replace(TEXT("\\"), TEXT("/"));
                    PluginSettings->ArchiveDirectory.Path = JsonObject->GetStringField(TEXT("GameDirectory")).Replace(TEXT("\\"), TEXT("/"));

					FString GameDirectory = JsonObject->GetStringField(TEXT("GameDirectory"));

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
                                            FAesKey NewKey;
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

void FJsonAsAssetSettingsDetails::EditEncryption(TWeakObjectPtr<UJsonAsAssetSettings> Settings,
	IDetailLayoutBuilder& DetailBuilder) {
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
		.OnClicked_Lambda([this, Settings]
		{
			UJsonAsAssetSettings* PluginSettings = GetMutableDefault<UJsonAsAssetSettings>();
			FHttpModule* HttpModule = &FHttpModule::Get();

#if ENGINE_MAJOR_VERSION >= 5
			const TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
#else
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule->CreateRequest();
#endif
			
			Request->SetURL("https://fortnitecentral.genxgames.gg/api/v1/aes");
			Request->SetVerb(TEXT("GET"));

#if ENGINE_MAJOR_VERSION >= 5
			const TSharedPtr<IHttpResponse> Response = FRemoteUtilities::ExecuteRequestSync(Request);
#else
			const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response = FRemoteUtilities::ExecuteRequestSync(Request);
#endif
			if (!Response.IsValid()) return FReply::Handled();

			PluginSettings->DynamicKeys.Empty();

			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			TSharedPtr<FJsonObject> JsonObject;
			
			if (FJsonSerializer::Deserialize(JsonReader, JsonObject)) {
				PluginSettings->ArchiveKey = JsonObject->GetStringField(TEXT("mainKey"));

				for (const TSharedPtr<FJsonValue> Value : JsonObject->GetArrayField(TEXT("dynamicKeys"))) {
					const TSharedPtr<FJsonObject> Object = Value->AsObject();

					FString GUID = Object->GetStringField(TEXT("guid"));
					FString Key = Object->GetStringField(TEXT("key"));

					PluginSettings->DynamicKeys.Add(FAesKey(GUID, Key));
				}
			}

			SavePluginConfig(PluginSettings);

			FString LocalExportDirectory = PluginSettings->ExportDirectory.Path;

			if (LocalExportDirectory != "" && LocalExportDirectory.Contains("/")) {
				FString DataFolder; {
					if (LocalExportDirectory.EndsWith("/")) LocalExportDirectory.Split("/", &DataFolder, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					else DataFolder = LocalExportDirectory;

					DataFolder.Split("/", &DataFolder, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd); {
						DataFolder = DataFolder + "/.data";
					}
				}

#if ENGINE_MAJOR_VERSION >= 5
				const TSharedRef<IHttpRequest> MappingsURLRequest = HttpModule->CreateRequest();
#else
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MappingsURLRequest = HttpModule->CreateRequest();
#endif

				MappingsURLRequest->SetURL("https://fortnitecentral.genxgames.gg/api/v1/mappings");
				MappingsURLRequest->SetVerb(TEXT("GET"));
				
#if ENGINE_MAJOR_VERSION >= 5
				const TSharedPtr<IHttpResponse> MappingsURLResponse = FRemoteUtilities::ExecuteRequestSync(MappingsURLRequest);
#else
				const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> MappingsURLResponse = FRemoteUtilities::ExecuteRequestSync(MappingsURLRequest);
#endif

				if (!MappingsURLResponse.IsValid()) return FReply::Handled();

				TSharedRef<TJsonReader<>> MappingsJsonReader = TJsonReaderFactory<>::Create(MappingsURLResponse->GetContentAsString());
				TArray<TSharedPtr<FJsonValue>> MappingsJsonArray;
				
				if (FJsonSerializer::Deserialize(MappingsJsonReader, MappingsJsonArray)) {
					TSharedPtr<FJsonValue> MappingsValue; {
						if (MappingsJsonArray.IsValidIndex(1)) MappingsValue = MappingsJsonArray[1];
						else MappingsValue = MappingsJsonArray[0];
					}

					if (MappingsValue == nullptr) return FReply::Handled();
					TSharedPtr<FJsonObject> MappingsObject = MappingsValue->AsObject();

					FString FileName = MappingsObject->GetStringField(TEXT("fileName"));
					FString URL = MappingsObject->GetStringField(TEXT("url"));

					auto OnRequestComplete = [DataFolder, FileName](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
						if (bWasSuccessful) {
							FFileHelper::SaveArrayToFile(Response->GetContent(), *(DataFolder + "/" + FileName));

							UJsonAsAssetSettings* PluginSettings = GetMutableDefault<UJsonAsAssetSettings>();

							PluginSettings->MappingFilePath.FilePath = DataFolder + "/" + FileName;
							
							SavePluginConfig(PluginSettings);
						}
					};

					TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MappingsRequest = FHttpModule::Get().CreateRequest();
					MappingsRequest->SetVerb("GET");
					MappingsRequest->SetURL(URL);
					MappingsRequest->OnProcessRequestComplete().BindLambda(OnRequestComplete);
					MappingsRequest->ProcessRequest();
				}
			}

			return FReply::Handled();
		}).IsEnabled_Lambda([this, Settings]()
		{
			return Settings->bEnableLocalFetch;
		})
	];
}

#undef LOCTEXT_NAMESPACE
