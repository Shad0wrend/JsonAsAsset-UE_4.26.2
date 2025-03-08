/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Importer.h"
#include "NiagaraParameterCollectionImporter.h"
#include "NiagaraParameterCollection.h"

class CNiagaraParameterCollectionDerived : public UNiagaraParameterCollection {
public:
    void SetSourceMaterialCollection(TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection);
    void SetCompileId(FGuid Guid);
    void SetNamespace(FName InNamespace);
    void AddAParameter(FNiagaraVariable Parameter);
};

class INiagaraParameterCollectionImporter : public IImporter {
public:
    INiagaraParameterCollectionImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects):
        IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects) {
    }

    virtual bool Import() override;
};

REGISTER_IMPORTER(INiagaraParameterCollectionImporter, {
    "NiagaraParameterCollection"
});