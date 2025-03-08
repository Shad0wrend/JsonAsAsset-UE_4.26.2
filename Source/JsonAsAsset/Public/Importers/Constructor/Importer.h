// Copyright JAA Contributors 2024-2025

#pragma once

#include "../../Utilities/Serializers/PropertyUtilities.h"
#include "../../Utilities/Serializers/ObjectUtilities.h"
#include "Utilities/AppStyleCompatibility.h"
#include "Utilities/EngineUtilities.h"
#include "Utilities/JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "CoreMinimal.h"

extern TArray<FString> ImporterAcceptedTypes;

#define REGISTER_IMPORTER(ImporterClass, ...) \
namespace { \
    struct FAutoRegister_##ImporterClass { \
        FAutoRegister_##ImporterClass() { \
            IImporter::GetFactoryRegistry().Add( TArray<FString>{ __VA_ARGS__ }, &IImporter::CreateImporter<ImporterClass> ); \
        } \
    }; \
    static FAutoRegister_##ImporterClass AutoRegister_##ImporterClass; \
}

FORCEINLINE uint32 GetTypeHash(const TArray<FString>& Array)
{
    uint32 Hash = 0;
    for (const FString& Str : Array)
    {
        Hash = HashCombine(Hash, GetTypeHash(Str));
    }
    return Hash;
}

/* Global handler for converting JSON to assets */
class IImporter {
public:
    /* Constructors ---------------------------------------------------------------------- */
    IImporter()
        : Package(nullptr), OutermostPkg(nullptr), ParentObject(nullptr),
          PropertySerializer(nullptr), GObjectSerializer(nullptr) {}

    // Importer Constructor
    IImporter(const FString& FileName, const FString& FilePath, 
              const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, 
              UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects = {});

    virtual ~IImporter() {}

    /* Easy way to find importers ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    using ImporterFactoryDelegate = TFunction<IImporter*(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& Exports)>;

    static TMap<TArray<FString>, ImporterFactoryDelegate>& GetFactoryRegistry() {
        static TMap<TArray<FString>, ImporterFactoryDelegate> Registry;
        return Registry;
    }

    template <typename T>
    static IImporter* CreateImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& Exports) {
        return new T(FileName, FilePath, JsonObject, Package, OutermostPkg, Exports);
    }

    static ImporterFactoryDelegate* FindFactoryForAssetType(const FString& AssetType)
    {
        for (auto& Pair : GetFactoryRegistry())
        {
            if (Pair.Key.Contains(AssetType))
            {
                return &Pair.Value;
            }
        }
        return nullptr;
    }
    
protected:
    /* Class variables ------------------------------------------------------------------ */
    TArray<TSharedPtr<FJsonValue>> AllJsonObjects;
    TSharedPtr<FJsonObject> JsonObject;
    FString FileName;
    FString FilePath;
    UPackage* Package;
    UPackage* OutermostPkg;

    /* ----------------------------------------------------------------------------------- */
    
public:
    /*
    * Overriden in child classes.
    * Returns false if failed.
    */
    virtual bool Import() {
        return false;
    }

public:
    /* Accepted Types ---------------------------------------------------------------------- */
    static TArray<FString> GetAcceptedTypes() {
        return ImporterAcceptedTypes;
    }

    static bool CanImport(const FString& ImporterType) {
        return ImporterAcceptedTypes.Contains(ImporterType)

        /* Added because we want to support as much sound classes as possible. SoundNode and SoundWave shouldn't be handled here */
        || ImporterType.StartsWith("Sound") && ImporterType != "SoundWave" && !ImporterType.StartsWith("SoundNode"); 
    }

    static bool CanImportAny(TArray<FString>& Types) {
        for (FString& Type : Types) {
            if (CanImport(Type)) return true;
        }
        return false;
    }

public:
    /* LoadObject functions ---------------------------------------------------------------------- */
    template<class T = UObject>
    void LoadObject(const TSharedPtr<FJsonObject>* PackageIndex, TObjectPtr<T>& Object);

    template<class T = UObject>
    TArray<TObjectPtr<T>> LoadObject(const TArray<TSharedPtr<FJsonValue>>& PackageArray, TArray<TObjectPtr<T>> Array);

    /* LoadObject functions ---------------------------------------------------------------------- */
public:
    void ImportReference(const FString& File) const;
    bool ImportExports(TArray<TSharedPtr<FJsonValue>> Exports, FString File, bool bHideNotifications = false) const;

public:
    TArray<TSharedPtr<FJsonValue>> GetObjectsWithTypeStartingWith(const FString& StartsWithStr);

    UObject* ParentObject;
    
protected:
    bool HandleAssetCreation(UObject* Asset) const;
    void SavePackage() const;

    TMap<FName, FExportData> CreateExports();

    // Handle edit changes, and add it to the content browser
    // Shortcut to calling SavePackage and HandleAssetCreation
    bool OnAssetCreation(UObject* Asset) const;

    static FName GetExportNameOfSubobject(const FString& PackageIndex);
    TArray<TSharedPtr<FJsonValue>> FilterExportsByOuter(const FString& Outer);
    TSharedPtr<FJsonValue> GetExportByObjectPath(const TSharedPtr<FJsonObject>& Object);

    /* ------------------------------------ Object Serializer and Property Serializer ------------------------------------ */
public:
    FORCEINLINE UObjectSerializer* GetObjectSerializer() const { return GObjectSerializer; }

    template <class T = UObject>
    TObjectPtr<T> DownloadWrapper(TObjectPtr<T> InObject, FString Type, FString Name, FString Path);

protected:
    UPropertySerializer* PropertySerializer;
    UObjectSerializer* GObjectSerializer;

    void SetupSerializer(UObject* ParentAsset) const
    {
        UObjectSerializer* ObjectSerializer = GetObjectSerializer();
        ObjectSerializer->SetPackageForDeserialization(Package);
        ObjectSerializer->SetExportForDeserialization(JsonObject);
        ObjectSerializer->ParentAsset = ParentAsset;  
    };
    /* ------------------------------------ Object Serializer and Property Serializer ------------------------------------ */
};