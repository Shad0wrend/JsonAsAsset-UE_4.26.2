/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Utilities/AssetUtilities.h"

#include "Importers/Constructor/Importer.h"

#include "Utilities/Textures/TextureCreatorUtilities.h"

#include "Curves/CurveLinearColor.h"
#include "Sound/SoundNode.h"
#include "Engine/SubsurfaceProfile.h"
#include "Materials/MaterialParameterCollection.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Settings/JsonAsAssetSettings.h"
#include "Dom/JsonObject.h"

#include "UObject/SavePackage.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/RemoteUtilities.h"
#include "PluginUtils.h"

/* CreateAssetPackage Implementations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
UPackage* FAssetUtilities::CreateAssetPackage(const FString& FullPath) {
	UPackage* Package = CreatePackage(
		/* 4.25, 4.26.0 and below need an Outer */
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26) || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 26 && ENGINE_PATCH_VERSION == 0)
		nullptr, 
#endif
		*FullPath);
	Package->FullyLoad();

	return Package;
}

UPackage* FAssetUtilities::CreateAssetPackage(const FString& Name, const FString& OutputPath, UPackage*& OutOutermostPkg) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();
	
	FString ModifiablePath;

	/* References Automatically Formatted */
	if (!OutputPath.StartsWith("/Game/") && !OutputPath.StartsWith("/Plugins/") && OutputPath.Contains("/Content/")) {
		OutputPath.Split(*(Settings->ExportDirectory.Path + "/"), nullptr, &ModifiablePath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		ModifiablePath.Split("/", nullptr, &ModifiablePath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		ModifiablePath.Split("/", &ModifiablePath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		/* Ex: RestPath: Plugins/Folder/BaseTextures */
		/* Ex: RestPath: Content/SecondaryFolder */
		bool bIsPlugin = ModifiablePath.StartsWith("Plugins");

		/* Plugins/Folder/BaseTextures -> Folder/BaseTextures */
		if (bIsPlugin) {
			FString PluginName = ModifiablePath;
			FString RemainingPath;
			/* PluginName = TestName */
			/* RemainingPath = SetupAssets/Materials */
			ModifiablePath.Split("/Content/", &PluginName, &RemainingPath, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			PluginName.Split("/", nullptr, &PluginName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

			/* /PluginName/Materials */
			ModifiablePath = PluginName + "/" + RemainingPath;
		}
		/* Content/SecondaryFolder -> Game/SecondaryFolder */
		else {
			ModifiablePath = ModifiablePath.Replace(TEXT("Content"), TEXT("Game"));
		}

		ModifiablePath = "/" + ModifiablePath + "/";

		/* Check if plugin exists */
		if (bIsPlugin) {
			FString PluginName;
			ModifiablePath.Split("/", nullptr, &PluginName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			PluginName.Split("/", &PluginName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);

			if (IPluginManager::Get().FindPlugin(PluginName) == nullptr)
				CreatePlugin(PluginName);
		}
	} else {
		FString RootName; {
			OutputPath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		}

		if (RootName != "Game" && RootName != "Engine" && IPluginManager::Get().FindPlugin(RootName) == nullptr) {
			CreatePlugin(RootName);
		}

		ModifiablePath = OutputPath;
		ModifiablePath.Split("/", &ModifiablePath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		ModifiablePath = ModifiablePath + "/";
	}

	const FString PathWithGame = ModifiablePath + Name;
	UPackage* Package = CreateAssetPackage(*PathWithGame);
	OutOutermostPkg = Package->GetOutermost();
	Package->FullyLoad();

	return Package;
}

UPackage* FAssetUtilities::CreateAssetPackage(const FString& Name, const FString& OutputPath) {
	UPackage* Ignore = nullptr; /* Put here because &nullptr doesn't work */
	
	return CreateAssetPackage(Name, OutputPath, Ignore);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
template bool FAssetUtilities::ConstructAsset<UMaterialInterface>(const FString& Path, const FString& Type, TObjectPtr<UMaterialInterface>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<USubsurfaceProfile>(const FString& Path, const FString& Type, TObjectPtr<USubsurfaceProfile>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<UTexture>(const FString& Path, const FString& Type, TObjectPtr<UTexture>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<UAnimSequence>(const FString& Path, const FString& Type, TObjectPtr<UAnimSequence>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<UMaterialParameterCollection>(const FString& Path, const FString& Type, TObjectPtr<UMaterialParameterCollection>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<USoundWave>(const FString& Path, const FString& Type, TObjectPtr<USoundWave>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<UObject>(const FString& Path, const FString& Type, TObjectPtr<UObject>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<UMaterialFunctionInterface>(const FString& Path, const FString& Type, TObjectPtr<UMaterialFunctionInterface>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<USoundNode>(const FString& Path, const FString& Type, TObjectPtr<USoundNode>& OutObject, bool& bSuccess);
template bool FAssetUtilities::ConstructAsset<UCurveLinearColor>(const FString& Path, const FString& Type, TObjectPtr<UCurveLinearColor>& OutObject, bool& bSuccess);

/* Importing assets from Local Fetch */
template <typename T>
bool FAssetUtilities::ConstructAsset(const FString& Path, const FString& Type, TObjectPtr<T>& OutObject, bool& bSuccess) {
	/* Skip if no type provided */
	if (Type == "") {
		return false;
	}

	/* Manually handled asset types */
	const bool bIsTexture = Type ==
			"Texture2D" ||
			Type == "TextureRenderTarget2D" ||
			Type == "TextureCube" ||
			Type == "VolumeTexture";

	/* Supported Assets */
	if (IImporter::CanImport(Type, true) || bIsTexture) {
		if (bIsTexture) {
			UTexture* Texture;
			FString NewPath = Path;

			FString RootName; {
				NewPath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
				RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			}

			/* Missing Plugin: Create it */
			if (RootName != "Game" && RootName != "Engine" && IPluginManager::Get().FindPlugin(RootName) == nullptr) {
				CreatePlugin(RootName);
			}

			bSuccess = Construct_TypeTexture(NewPath, Path, Texture);
			if (bSuccess) OutObject = Cast<T>(Texture);

			return true;
		}
		
		const TSharedPtr<FJsonObject> Response = API_RequestExports(Path);
		if (Response == nullptr || Path.IsEmpty()) return true;

		if (Response->HasField(TEXT("errored"))) {
			UE_LOG(LogJson, Log, TEXT("Error from response \"%s\""), *Path);
			return true;
		}

		TSharedPtr<FJsonObject> JsonObject = Response->GetArrayField(TEXT("jsonOutput"))[0]->AsObject();
		FString PackagePath;
		FString AssetName;
		Path.Split(".", &PackagePath, &AssetName);

		if (JsonObject) {
			FString NewPath = PackagePath;

			FString RootName; {
				NewPath.Split("/", nullptr, &RootName, ESearchCase::IgnoreCase, ESearchDir::FromStart);
				RootName.Split("/", &RootName, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
			}

			if (RootName != "Game" && RootName != "Engine" && IPluginManager::Get().FindPlugin(RootName) == nullptr) {
				CreatePlugin(RootName);
			}

			UPackage* Package = CreateAssetPackage(*NewPath);
			Package->FullyLoad();

			/* Import asset by IImporter */
			bSuccess = IImporter::ReadExportsAndImport(Response->GetArrayField(TEXT("jsonOutput")), PackagePath, true);

			/* Define found object */
			OutObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *Path));

			return OutObject != nullptr;
		}
	}

	return false;
}

bool FAssetUtilities::Construct_TypeTexture(const FString& Path, const FString& RealPath, UTexture*& OutTexture) {
	if (Path.IsEmpty()) {
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject = API_RequestExports(RealPath);
	if (JsonObject == nullptr) {
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Response = JsonObject->GetArrayField(TEXT("jsonOutput"));
	if (Response.Num() == 0) {
		return false;
	}

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();
	TSharedPtr<FJsonObject> JsonExport = Response[0]->AsObject();
	FString Type = JsonExport->GetStringField(TEXT("Type"));
	UTexture* Texture = nullptr;
	TArray<uint8> Data = TArray<uint8>();

	/* ~~~~~~~~~~~~~~~ Download Texture Data ~~~~~~~~~~~~ */
	if (Type != "TextureRenderTarget2D") {
		FHttpModule* HttpModule = &FHttpModule::Get();
#if ENGINE_MAJOR_VERSION >= 5
		const TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
#else
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
#endif

		HttpRequest->SetURL(Settings->LocalFetchUrl + "/api/export?path=" + RealPath);
		HttpRequest->SetHeader("content-type", "application/octet-stream");
		HttpRequest->SetVerb(TEXT("GET"));

#if ENGINE_MAJOR_VERSION >= 5
		const TSharedPtr<IHttpResponse> HttpResponse = FRemoteUtilities::ExecuteRequestSync(HttpRequest);
#else
		const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse = FRemoteUtilities::ExecuteRequestSync(HttpRequest);
#endif
		if (!HttpResponse.IsValid() || HttpResponse->GetResponseCode() != 200)
			return false;

		if (HttpResponse->GetContentType().StartsWith("application/json; charset=utf-8")) {
			return false;
		}

		Data = HttpResponse->GetContent();
		if (Data.Num() == 0) {
			return false;
		}
	}

	FString PackagePath;
	FString AssetName; {
		Path.Split(".", &PackagePath, &AssetName);
	}

	UPackage* Package = CreateAssetPackage(*PackagePath);
	UPackage* OutermostPkg = Package->GetOutermost();
	Package->FullyLoad();

	FTextureCreatorUtilities TextureCreator = FTextureCreatorUtilities(AssetName, Path, Package, OutermostPkg);

	if (Type == "Texture2D") {
		TextureCreator.CreateTexture2D(Texture, Data, JsonExport);
	}
	if (Type == "TextureCube") {
		TextureCreator.CreateTextureCube(Texture, Data, JsonExport);
	}
	if (Type == "VolumeTexture") {
		TextureCreator.CreateVolumeTexture(Texture, Data, JsonExport);
	}
	if (Type == "TextureRenderTarget2D") {
		TextureCreator.CreateRenderTarget2D(Texture, JsonExport->GetObjectField(TEXT("Properties")));
	}

	if (Texture == nullptr) {
		return false;
	}

	FAssetRegistryModule::AssetCreated(Texture);
	if (!Texture->MarkPackageDirty()) {
		return false;
	}

	Package->SetDirtyFlag(true);
	Texture->PostEditChange();
	Texture->AddToRoot();
	Package->FullyLoad();

	/* Save texture */
	if (Settings->AssetSettings.bSavePackagesOnImport) {
		const FString PackageName = Package->GetName();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
#if ENGINE_MAJOR_VERSION >= 5
		FSavePackageArgs SaveArgs; {
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
		}
		
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
#else
		UPackage::SavePackage(Package, nullptr, RF_Standalone, *PackageFileName);
#endif
	}

	OutTexture = Texture;

	return true;
}

void FAssetUtilities::CreatePlugin(FString PluginName) {
	/* Plugin creation is different between UE5 and UE4 */
#if ENGINE_MAJOR_VERSION >= 5
	FPluginUtils::FNewPluginParamsWithDescriptor CreationParams;
	CreationParams.Descriptor.bCanContainContent = true;

	CreationParams.Descriptor.FriendlyName = PluginName;
	CreationParams.Descriptor.Version = 1;
	CreationParams.Descriptor.VersionName = TEXT("1.0");
	CreationParams.Descriptor.Category = TEXT("Other");

	FText FailReason;
	FPluginUtils::FLoadPluginParams LoadParams;
	LoadParams.bEnablePluginInProject = true;
	LoadParams.bUpdateProjectPluginSearchPath = true;
	LoadParams.bSelectInContentBrowser = false;

	FPluginUtils::CreateAndLoadNewPlugin(PluginName, FPaths::ProjectPluginsDir(), CreationParams, LoadParams);
#else
	FPluginUtils::FNewPluginParams CreationParams;
	CreationParams.bCanContainContent = true;

	FText FailReason;
	FPluginUtils::FMountPluginParams LoadParams;
	LoadParams.bEnablePluginInProject = true;
	LoadParams.bUpdateProjectPluginSearchPath = true;
	LoadParams.bSelectInContentBrowser = false;

	FPluginUtils::CreateAndMountNewPlugin(PluginName, FPaths::ProjectPluginsDir(), CreationParams, LoadParams, FailReason);
#endif

#define LOCTEXT_NAMESPACE "UMG"
#if WITH_EDITOR
	/* Setup notification's arguments */
	FFormatNamedArguments Args;
	Args.Add(TEXT("PluginName"), FText::FromString(PluginName));

	/* Create notification */
	FNotificationInfo Info(FText::Format(LOCTEXT("PluginCreated", "Plugin Created: {PluginName}"), Args));
	Info.ExpireDuration = 10.0f;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = false;
	Info.WidthOverride = FOptionalSize(350);
	SetNotificationSubText(Info, FText::FromString(FString("Created successfully")));
	
	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
#endif
#undef LOCTEXT_NAMESPACE
}

TSharedPtr<FJsonObject> FAssetUtilities::API_RequestExports(const FString& Path, const FString& FetchPath) {
	FHttpModule* HttpModule = &FHttpModule::Get();

#if ENGINE_MAJOR_VERSION >= 5
	const TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();
#else
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule->CreateRequest();
#endif

	FString PackagePath;
	FString AssetName;
	Path.Split(".", &PackagePath, &AssetName);

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

#if ENGINE_MAJOR_VERSION >= 5
	const TSharedRef<IHttpRequest> NewRequest = HttpModule->CreateRequest();
#else
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> NewRequest = HttpModule->CreateRequest();
#endif
	NewRequest->SetURL(Settings->LocalFetchUrl + FetchPath + Path);
	NewRequest->SetVerb(TEXT("GET"));

#if ENGINE_MAJOR_VERSION >= 5
	const TSharedPtr<IHttpResponse> NewResponse = FRemoteUtilities::ExecuteRequestSync(NewRequest);
#else
	const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> NewResponse = FRemoteUtilities::ExecuteRequestSync(NewRequest);
#endif
	if (!NewResponse.IsValid()) return TSharedPtr<FJsonObject>();

	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(NewResponse->GetContentAsString());
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject)) {
		return JsonObject;
	}

	return TSharedPtr<FJsonObject>();
}
