/* Copyright JAA Contributors 2024~2025 */

#pragma once

#include "../../Utilities/Serializers/PropertyUtilities.h"
#include "../../Utilities/Serializers/ObjectUtilities.h"
#include "Utilities/AppStyleCompatibility.h"
#include "Utilities/EngineUtilities.h"
#include "Utilities/JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "CoreMinimal.h"

/* AssetType/Category ~ Defined in CPP */
extern TMap<FString, TArray<FString>> ImporterTemplatedTypes;

inline TArray<FString> BlacklistedLocalFetchTypes = {
    "AnimSequence",
    "AnimMontage",
};

FORCEINLINE uint32 GetTypeHash(const TArray<FString>& Array) {
    uint32 Hash = 0;
    
    for (const FString& Str : Array) {
        Hash = HashCombine(Hash, GetTypeHash(Str));
    }
    
    return Hash;
}

#define REGISTER_IMPORTER(ImporterClass, AcceptedTypes, Category) \
namespace { \
    struct FAutoRegister_##ImporterClass { \
        FAutoRegister_##ImporterClass() { \
            IImporter::FImporterRegistrationInfo Info( FString(Category), &IImporter::CreateImporter<ImporterClass> ); \
            IImporter::GetFactoryRegistry().Add(AcceptedTypes, Info); \
        } \
    }; \
    static FAutoRegister_##ImporterClass AutoRegister_##ImporterClass; \
}

/* Global handler for converting JSON to assets */
class IImporter {
public:
    /* Constructors ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    IImporter()
        : Package(nullptr), OutermostPkg(nullptr), AssetClass(nullptr), ParentObject(nullptr),
          PropertySerializer(nullptr), GObjectSerializer(nullptr) {}

    /* Importer Constructor */
    IImporter(const FString& FileName, const FString& FilePath, 
              const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, 
              UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects = {}, UClass* AssetClass = nullptr);

    virtual ~IImporter() {}

    /* Easy way to find importers ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    using ImporterFactoryDelegate = TFunction<IImporter*(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& Exports, UClass* AssetClass)>;

    template <typename T>
    static IImporter* CreateImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& Exports, UClass* AssetClass) {
        return new T(FileName, FilePath, JsonObject, Package, OutermostPkg, Exports, AssetClass);
    }

    /* Registration info for an importer */
    struct FImporterRegistrationInfo {
        FString Category;
        ImporterFactoryDelegate Factory;

        FImporterRegistrationInfo(const FString& InCategory, const ImporterFactoryDelegate& InFactory)
            : Category(InCategory)
            , Factory(InFactory)
        {
        }

        FImporterRegistrationInfo() = default;
    };

    static TMap<TArray<FString>, FImporterRegistrationInfo>& GetFactoryRegistry() {
        static TMap<TArray<FString>, FImporterRegistrationInfo> Registry;
        
        return Registry;
    }

    static ImporterFactoryDelegate* FindFactoryForAssetType(const FString& AssetType) {
        for (auto& Pair : GetFactoryRegistry()) {
            if (Pair.Key.Contains(AssetType)) {
                return &Pair.Value.Factory;
            }
        }
        
        return nullptr;
    }
    
protected:
    /* Class variables ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    TArray<TSharedPtr<FJsonValue>> AllJsonObjects;
    TSharedPtr<FJsonObject> JsonObject;
    FString FileName;
    FString FilePath;
    UPackage* Package;
    UPackage* OutermostPkg;

    TSharedPtr<FJsonObject> AssetData;
    UClass* AssetClass;

    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    
public:
    /*
    * Overriden in child classes.
    * Returns false if failed.
    */
    virtual bool Import() {
        return false;
    }

public:
    /* Accepted Types ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    static bool CanImportWithLocalFetch(const FString& ImporterType) {
        if (BlacklistedLocalFetchTypes.Contains(ImporterType)) {
            return false;
        }

        return true;
    }
    
    static bool CanImport(const FString& ImporterType, bool IsLocalFetch = false, const UClass* Class = nullptr) {
        /* Blacklists for Local Fetch importing */
        if (IsLocalFetch) {
            if (!CanImportWithLocalFetch(ImporterType)) {
                return false;
            }
        }
        
        if (FindFactoryForAssetType(ImporterType)) {
            return true;
        };
        
        for (const TPair<FString, TArray<FString>>& Pair : ImporterTemplatedTypes) {
            if (Pair.Value.Contains(ImporterType)) {
                return true;
            }
        }

        if (!Class) {
            Class = FindObject<UClass>(ANY_PACKAGE, *ImporterType);
        }

        if (Class == nullptr) return false;
        
        return Class->IsChildOf(UDataAsset::StaticClass());
    }

    static bool CanImportAny(TArray<FString>& Types) {
        for (FString& Type : Types) {
            if (CanImport(Type)) return true;
        }
        return false;
    }

public:
    /* Loads a single <T> object ptr */
    template<class T = UObject>
    void LoadObject(const TSharedPtr<FJsonObject>* PackageIndex, TObjectPtr<T>& Object);

    /* Loads an array of <T> object ptrs */
    template<class T = UObject>
    TArray<TObjectPtr<T>> LoadObject(const TArray<TSharedPtr<FJsonValue>>& PackageArray, TArray<TObjectPtr<T>> Array);

public:
    /* Sends off to the ReadExportsAndImport function once read */
    static void ImportReference(const FString& File);

    /*
     * Searches for importable asset types and imports them.
     */
    static bool ReadExportsAndImport(TArray<TSharedPtr<FJsonValue>> Exports, FString File, bool bHideNotifications = false);

public:
    TArray<TSharedPtr<FJsonValue>> GetObjectsWithTypeStartingWith(const FString& StartsWithStr);

    UObject* ParentObject;
    
protected:
    /* This is called at the end of asset creation, bringing the user to the asset and fully loading it */
    bool HandleAssetCreation(UObject* Asset) const;
    void SavePackage() const;

    TMap<FName, FExportData> CreateExports();

    /*
     * Handle edit changes, and add it to the content browser
     */
    bool OnAssetCreation(UObject* Asset) const;

    static FName GetExportNameOfSubobject(const FString& PackageIndex);
    TArray<TSharedPtr<FJsonValue>> FilterExportsByOuter(const FString& Outer);
    TSharedPtr<FJsonValue> GetExportByObjectPath(const TSharedPtr<FJsonObject>& Object);

    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Object Serializer and Property Serializer ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
public:
    FORCEINLINE UObjectSerializer* GetObjectSerializer() const { return GObjectSerializer; }

    /* Function to check if an asset needs to be imported. Once imported, the asset will be set and returned. */
    template <class T = UObject>
    TObjectPtr<T> DownloadWrapper(TObjectPtr<T> InObject, FString Type, FString Name, FString Path);

protected:
    UPropertySerializer* PropertySerializer;
    UObjectSerializer* GObjectSerializer;

    void DeserializeExports(UObject* ParentAsset) const {
        UObjectSerializer* ObjectSerializer = GetObjectSerializer();
        ObjectSerializer->SetPackageForDeserialization(Package);
        ObjectSerializer->SetExportForDeserialization(JsonObject);
        ObjectSerializer->ParentAsset = ParentAsset;
        
        ObjectSerializer->DeserializeExports(AllJsonObjects);
    };
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Object Serializer and Property Serializer ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
};


