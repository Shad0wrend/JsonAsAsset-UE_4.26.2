/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Constructor/Importer.h"

#include "Settings/JsonAsAssetSettings.h"

/* Content Browser Modules */
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"

/* Utilities */
#include "Utilities/AssetUtilities.h"

#include "Misc/MessageDialog.h"
#include "UObject/SavePackage.h"

/* Slate Icons */
#include "Styling/SlateIconFinder.h"

/* Particle System Importing is not finalized */
#ifndef JSONASASSET_PARTICLESYSTEM_ALLOW
#define JSONASASSET_PARTICLESYSTEM_ALLOW 0
#endif

#include "Importers/Types/DataAssetImporter.h"

/* Templated Class */
#include "Importers/Constructor/TemplatedImporter.h"

/* ~~~~~~~~~~~~~ Templated Engine Classes ~~~~~~~~~~~~~ */
#include "Materials/MaterialParameterCollection.h"
#include "Engine/SubsurfaceProfile.h"
#include "Curves/CurveLinearColor.h"
#include "Logging/MessageLog.h"
#include "Sound/SoundNode.h"
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define LOCTEXT_NAMESPACE "IImporter"

/* Importer Constructor */
IImporter::IImporter(const FString& FileName, const FString& FilePath, 
		  const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, 
		  UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects,
		  UClass* AssetClass)
	: AllJsonObjects(AllJsonObjects), JsonObject(JsonObject), FileName(FileName),
	  FilePath(FilePath), Package(Package), OutermostPkg(OutermostPkg), AssetClass(AssetClass),
	  ParentObject(nullptr)
{
	PropertySerializer = NewObject<UPropertySerializer>();
	GObjectSerializer = NewObject<UObjectSerializer>();
	GObjectSerializer->SetPropertySerializer(PropertySerializer);
	
	/* Create Properties field if it doesn't exist */
	if (!JsonObject->HasField(TEXT("Properties"))) {
		JsonObject->SetObjectField(TEXT("Properties"), TSharedPtr<FJsonObject>());
	}

	AssetData = JsonObject->GetObjectField(TEXT("Properties"));

	/* Move asset properties defined outside "Properties" and move it inside */
	for (const auto& Pair : JsonObject->Values) {
		const FString& PropertyName = Pair.Key;
    
		if (!PropertyName.Equals(TEXT("Type")) &&
			!PropertyName.Equals(TEXT("Name")) &&
			!PropertyName.Equals(TEXT("Class")) &&
			!PropertyName.Equals(TEXT("Flags")) &&
			!PropertyName.Equals(TEXT("Properties")))
		{
			AssetData->SetField(PropertyName, Pair.Value);
		}
	}
}

/*
 * Define supported asset class names here
 *
 * An empty string "" is a separator line
 * A string starting with "# ..." is a category
 */
TArray<FString> ImporterAcceptedTypes = {
	"# Animation Assets", /* Animation Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"AnimSequence",
	"AnimMontage",

	"", /* separator */
	
	"BlendSpace",
	"BlendSpace1D",
	"AimOffsetBlendSpace",
	"AimOffsetBlendSpace1D",
	
	"# Curve Assets", /* Curve Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
	"CurveFloat",
	"CurveVector",
	"CurveLinearColor",
	"CurveLinearColorAtlas",

	"# Data Assets", /* Data Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"DataAsset",

	"", /* separator */

	"SlateBrushAsset",
	"SlateWidgetStyleAsset",

	"", /* separator */
	
	"AnimBoneCompressionSettings",
	"AnimCurveCompressionSettings",

	"# Landscape Assets", /* Landscape Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
		
	"LandscapeGrassType",
	
	"FoliageType_InstancedStaticMesh",
	"FoliageType_Actor",
	
	"# Table Assets", /* Data Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"CurveTable",
	"DataTable",
	"StringTable",
	
	"# Material Assets", /* Material Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"Material",
	"MaterialFunction",
	"MaterialInstanceConstant",
	"MaterialParameterCollection",

	"", /* separator */
	
	"SubsurfaceProfile",
	"NiagaraParameterCollection",
	
	"# Skeletal Assets", /* Skeletal Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"SkeletalMeshLODSettings",
	"Skeleton",

#if JSONASASSET_PARTICLESYSTEM_ALLOW
	"# Particle Assets", /* Particle Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"ParticleSystem",
#endif

	"# Physics Assets", /* Physics Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
	"PhysicsAsset",
	"PhysicalMaterial",

	"# Sound Assets", /* Sound Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"SoundCue",
	"ReverbEffect",
	"SoundAttenuation",
	"SoundConcurrency",
	"SoundClass",
	"SoundMix",
	"SoundModulationPatch",
	"SubmixEffectDynamicsProcessorPreset",

	"# Texture Assets", /* Texture Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"TextureRenderTarget2D",
	
	"# User Defined Assets", /* User Defined Assets ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	"UserDefinedEnum"
};

bool IImporter::ReadExportsAndImport(TArray<TSharedPtr<FJsonValue>> Exports, FString File, const bool bHideNotifications) {
	for (const TSharedPtr<FJsonValue>& ExportPtr : Exports) {
		TSharedPtr<FJsonObject> DataObject = ExportPtr->AsObject();

		FString Type = DataObject->GetStringField(TEXT("Type"));
		FString Name = DataObject->GetStringField(TEXT("Name"));

		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *Type);

		if (Class == nullptr) continue;

		/* If the class inherits DataAsset, it is importable */
		const bool InheritsDataAsset = Class->IsChildOf(UDataAsset::StaticClass());

		/* Check if this export can be imported */
		if (!(CanImport(Type) || InheritsDataAsset)) continue;

		/* Convert from relative path to full path */
		if (FPaths::IsRelative(File)) File = FPaths::ConvertRelativePathToFull(File);

		UPackage* LocalOutermostPkg;
		UPackage* LocalPackage = FAssetUtilities::CreateAssetPackage(Name, File, LocalOutermostPkg);

		/* Importer ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
		IImporter* Importer = nullptr;
		
		/* Try to find the importer using a factory delegate */
		if (const ImporterFactoryDelegate* Factory = FindFactoryForAssetType(Type)) {
			Importer = (*Factory)(Name, File, DataObject, LocalPackage, LocalOutermostPkg, Exports, Class);
		}

		/* If it inherits DataAsset, use the data asset importer */
		if (Importer == nullptr && InheritsDataAsset) {
			Importer = new IDataAssetImporter(Name, File, DataObject, LocalPackage, LocalOutermostPkg, Exports, Class);
		}

		/*
		 * By default, (with no existing importer) use the templated importer with the asset class.
		 */
		if (Importer == nullptr) {
			Importer = new ITemplatedImporter<UObject>(
				Name, File, DataObject, LocalPackage, LocalOutermostPkg, Exports, Class
			);
		}

		/* Import the asset ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
		bool Successful = false; {
			try {
				Successful = Importer->Import();
			} catch (const char* Exception) {
				UE_LOG(LogJson, Error, TEXT("Importer exception: %s"), *FString(Exception));
			}
		}

		if (bHideNotifications) {
			return Successful;
		}

		if (Successful) {
			UE_LOG(LogJson, Log, TEXT("Successfully imported \"%s\" as \"%s\""), *Name, *Type);
			
			if (!(Type == "AnimSequence" || Type == "AnimMontage"))
				Importer->SavePackage();

			/* Import Successful Notification */
			AppendNotification(
				FText::FromString("Imported Type: " + Type),
				FText::FromString(Name),
				2.0f,
				FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail")),
				SNotificationItem::CS_Success,
				false,
				350.0f
			);

			FMessageLog MessageLogger = FMessageLog(FName("JsonAsAsset"));

			MessageLogger.Message(EMessageSeverity::Info, FText::FromString("Imported Asset: " + Name + " (" + Type + ")"));
		} else {
			/* Import Failed Notification */
			AppendNotification(
				FText::FromString("Import Failed: " + Type),
				FText::FromString(Name),
				2.0f,
				FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail")),
				SNotificationItem::CS_Fail,
				false,
				350.0f
			);
		}
	}

	return true;
}

TArray<TSharedPtr<FJsonValue>> IImporter::GetObjectsWithTypeStartingWith(const FString& StartsWithStr) {
	TArray<TSharedPtr<FJsonValue>> FilteredObjects;

	for (const TSharedPtr<FJsonValue>& JsonObjectValue : AllJsonObjects) {
		if (JsonObjectValue->Type == EJson::Object) {
			TSharedPtr<FJsonObject> JsonObjectType = JsonObjectValue->AsObject();

			if (JsonObjectType.IsValid() && JsonObjectType->HasField(TEXT("Type"))) {
				FString TypeValue = JsonObjectType->GetStringField(TEXT("Type"));

				/* Check if the "Type" field starts with the specified string */
				if (TypeValue.StartsWith(StartsWithStr)) {
					FilteredObjects.Add(JsonObjectValue);
				}
			}
		}
	}

	return FilteredObjects;
}

bool IImporter::HandleAssetCreation(UObject* Asset) const {
	FAssetRegistryModule::AssetCreated(Asset);
	if (!Asset->MarkPackageDirty()) return false;
	
	Package->SetDirtyFlag(true);
	Asset->PostEditChange();
	Asset->AddToRoot();
	
	Package->FullyLoad();

	/* Browse to newly added Asset in the Content Browser */
	const TArray<FAssetData>& Assets = {Asset };
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(Assets);

	Asset->PostLoad();
	return true;
}

template <typename T>
TObjectPtr<T> IImporter::DownloadWrapper(TObjectPtr<T> InObject, FString Type, FString Name, FString Path) {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	FMessageLog MessageLogger = FMessageLog(FName("JsonAsAsset"));

	if (Settings->bEnableLocalFetch && (
		InObject == nullptr ||
			Settings->AssetSettings.TextureImportSettings.bDownloadExistingTextures &&
			Type == "Texture2D"
		)
	) {
		const UObject* DefaultObject = T::StaticClass()->ClassDefaultObject;

		if (DefaultObject != nullptr && !Name.IsEmpty() && !Path.IsEmpty()) {
			bool bRemoteDownloadStatus = false;

			/* Try importing the asset */
			if (FAssetUtilities::ConstructAsset(FSoftObjectPath(Type + "'" + Path + "." + Name + "'").ToString(), Type, InObject, bRemoteDownloadStatus)) {
				const FText AssetNameText = FText::FromString(Name);
				const FSlateBrush* IconBrush = FSlateIconFinder::FindCustomIconBrushForClass(FindObject<UClass>(nullptr, *("/Script/Engine." + Type)), TEXT("ClassThumbnail"));

				if (bRemoteDownloadStatus) {
					AppendNotification(
						FText::FromString("Locally Downloaded: " + Type),
						AssetNameText,
						2.0f,
						IconBrush,
						SNotificationItem::CS_Success,
						false,
						310.0f
					);

					MessageLogger.Message(EMessageSeverity::Info, FText::FromString("Downloaded asset: " + Name + " (" + Type + ")"));
				} else {
					AppendNotification(
						FText::FromString("Download Failed: " + Type),
						AssetNameText,
						5.0f,
						IconBrush,
						SNotificationItem::CS_Fail,
						false,
						310.0f
					);

					MessageLogger.Error(FText::FromString("Failed to download asset: " + Name + " (" + Type + ")"));
				}
			}
		}
	}

	return InObject;
}

template void IImporter::LoadObject<UMaterialInterface>(const TSharedPtr<FJsonObject>*, TObjectPtr<UMaterialInterface>&);
template void IImporter::LoadObject<USubsurfaceProfile>(const TSharedPtr<FJsonObject>*, TObjectPtr<USubsurfaceProfile>&);
template void IImporter::LoadObject<UTexture>(const TSharedPtr<FJsonObject>*, TObjectPtr<UTexture>&);
template void IImporter::LoadObject<UMaterialParameterCollection>(const TSharedPtr<FJsonObject>*, TObjectPtr<UMaterialParameterCollection>&);
template void IImporter::LoadObject<UAnimSequence>(const TSharedPtr<FJsonObject>*, TObjectPtr<UAnimSequence>&);
template void IImporter::LoadObject<USoundWave>(const TSharedPtr<FJsonObject>*, TObjectPtr<USoundWave>&);
template void IImporter::LoadObject<UObject>(const TSharedPtr<FJsonObject>*, TObjectPtr<UObject>&);
template void IImporter::LoadObject<UMaterialFunctionInterface>(const TSharedPtr<FJsonObject>*, TObjectPtr<UMaterialFunctionInterface>&);
template void IImporter::LoadObject<USoundNode>(const TSharedPtr<FJsonObject>*, TObjectPtr<USoundNode>&);

template <typename T>
void IImporter::LoadObject(const TSharedPtr<FJsonObject>* PackageIndex, TObjectPtr<T>& Object) {
	FString ObjectType, ObjectName, ObjectPath, Outer;
	PackageIndex->Get()->GetStringField(TEXT("ObjectName")).Split("'", &ObjectType, &ObjectName);
	PackageIndex->Get()->GetStringField(TEXT("ObjectPath")).Split(".", &ObjectPath, nullptr);

	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	/* Rare case of needing a GameName */
	if (!Settings->AssetSettings.GameName.IsEmpty()) {
		ObjectPath = ObjectPath.Replace(*(Settings->AssetSettings.GameName + "/Content"), TEXT("/Game"));
	}

	ObjectPath = ObjectPath.Replace(TEXT("Engine/Content"), TEXT("/Engine"));
	ObjectName = ObjectName.Replace(TEXT("'"), TEXT(""));

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", nullptr, &ObjectName);
	}

	if (ObjectName.Contains(".")) {
		ObjectName.Split(".", &Outer, &ObjectName);
	}

	/* Try to load object using the object path and the object name combined */
	TObjectPtr<T> LoadedObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *(ObjectPath + "." + ObjectName)));

	if (!Outer.IsEmpty())
	{
		AActor* NewLoadedObject = Cast<AActor>(ParentObject);
		auto Components = NewLoadedObject->GetComponents();
		
		for (UActorComponent* Component : Components)
		{
			if (ObjectName == Component->GetName())
			{
				LoadedObject = Cast<T>(Component);
			}
		}
	}
	
	/* Material Expression case */
	if (!LoadedObject && ObjectName.Contains("MaterialExpression")) {
		FString AssetName;
		ObjectPath.Split("/", nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		LoadedObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *(ObjectPath + "." + AssetName + ":" + ObjectName)));
	}

	Object = LoadedObject;

	if (!Object)
	{
		Object = DownloadWrapper(LoadedObject, ObjectType, ObjectName, ObjectPath);
	}
}

template TArray<TObjectPtr<UCurveLinearColor>> IImporter::LoadObject<UCurveLinearColor>(const TArray<TSharedPtr<FJsonValue>>&, TArray<TObjectPtr<UCurveLinearColor>>);

template <typename T>
TArray<TObjectPtr<T>> IImporter::LoadObject(const TArray<TSharedPtr<FJsonValue>>& PackageArray, TArray<TObjectPtr<T>> Array) {
	for (const TSharedPtr<FJsonValue>& ArrayElement : PackageArray) {
		const TSharedPtr<FJsonObject> ObjectPtr = ArrayElement->AsObject();

		FString ObjectType, ObjectName, ObjectPath;
		
		ObjectPtr->GetStringField(TEXT("ObjectName")).Split("'", &ObjectType, &ObjectName);
		ObjectPtr->GetStringField(TEXT("ObjectPath")).Split(".", &ObjectPath, nullptr);
		ObjectName = ObjectName.Replace(TEXT("'"), TEXT(""));

		TObjectPtr<T> LoadedObject = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *(ObjectPath + "." + ObjectName)));
		Array.Add(DownloadWrapper(LoadedObject, ObjectType, ObjectName, ObjectPath));
	}

	return Array;
}

void IImporter::ImportReference(const FString& File) {
	/* ~~~~  Parse JSON into UE JSON Reader ~~~~ */
	FString ContentBefore;
	FFileHelper::LoadFileToString(ContentBefore, *File);

	FString Content = FString(TEXT("{\"data\": "));
	Content.Append(ContentBefore);
	Content.Append(FString("}"));

	TSharedPtr<FJsonObject> JsonParsed;
	const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Content);
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {
		const TArray<TSharedPtr<FJsonValue>> DataObjects = JsonParsed->GetArrayField(TEXT("data"));

		ReadExportsAndImport(DataObjects, File);
	}
}

TMap<FName, FExportData> IImporter::CreateExports() {
	TMap<FName, FExportData> OutExports;

	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		TSharedPtr<FJsonObject> Object = TSharedPtr<FJsonObject>(Value->AsObject());

		FString ExType = Object->GetStringField(TEXT("Type"));
		FString Name = Object->GetStringField(TEXT("Name"));
		FString Outer = "None";

		if (Object->HasField(TEXT("Outer"))) {
			Outer = Object->GetStringField(TEXT("Outer"));
		}

		OutExports.Add(FName(Name), FExportData(ExType, Outer, Object));
	}

	return OutExports;
}

void IImporter::SavePackage() const {
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	/* Ensure the package is valid before proceeding */
	if (Package == nullptr) {
		UE_LOG(LogTemp, Error, TEXT("Package is null"));
		return;
	}

	const FString PackageName = Package->GetName();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	/* User option to save packages on import */
	if (Settings->AssetSettings.bSavePackagesOnImport) {
#if ENGINE_MAJOR_VERSION >= 5
		FSavePackageArgs SaveArgs; {
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.Error = GError;
			SaveArgs.SaveFlags = SAVE_NoError;
		}
		
		UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs);
#else
		UPackage::SavePackage(Package, nullptr, RF_Standalone, *PackageFileName);
#endif
	}
}

bool IImporter::OnAssetCreation(UObject* Asset) const {
	SavePackage();
	
	return HandleAssetCreation(Asset);
}

FName IImporter::GetExportNameOfSubobject(const FString& PackageIndex) {
	FString Name; {
		PackageIndex.Split("'", nullptr, &Name);
		Name.Split(":", nullptr, &Name);
		Name = Name.Replace(TEXT("'"), TEXT(""));
	}
	
	return FName(Name);
}

TArray<TSharedPtr<FJsonValue>> IImporter::FilterExportsByOuter(const FString& Outer) {
	TArray<TSharedPtr<FJsonValue>> ReturnValue = TArray<TSharedPtr<FJsonValue>>();

	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Value->AsObject());

		FString ExportOuter;
		if (ValueObject->TryGetStringField(TEXT("Outer"), ExportOuter) && ExportOuter == Outer) 
			ReturnValue.Add(TSharedPtr<FJsonValue>(Value));
	}

	return ReturnValue;
}

TSharedPtr<FJsonValue> IImporter::GetExportByObjectPath(const TSharedPtr<FJsonObject>& Object) {
	const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Object);

	FString StringIndex; {
		ValueObject->GetStringField(TEXT("ObjectPath")).Split(".", nullptr, &StringIndex);
	}

	return AllJsonObjects[FCString::Atod(*StringIndex)];
}

#undef LOCTEXT_NAMESPACE