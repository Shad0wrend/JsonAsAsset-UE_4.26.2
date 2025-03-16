/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Niagara/NiagaraParameterCollectionImporter.h"
#include "Materials/MaterialParameterCollection.h"
#include "Dom/JsonObject.h"

void CNiagaraParameterCollectionDerived::SetSourceMaterialCollection(TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection) {
    this->SourceMaterialCollection = MaterialParameterCollection;
}

void CNiagaraParameterCollectionDerived::SetCompileId(FGuid Guid) {
    this->CompileId = Guid;
}

void CNiagaraParameterCollectionDerived::SetNamespace(FName InNamespace) {
    this->Namespace = InNamespace;
}

void CNiagaraParameterCollectionDerived::AddAParameter(FNiagaraVariable Parameter) {
    this->Parameters.Add(Parameter);
}

bool INiagaraParameterCollectionImporter::Import() {
    CNiagaraParameterCollectionDerived* NiagaraParameterCollection = Cast<CNiagaraParameterCollectionDerived>(
        NewObject<UNiagaraParameterCollection>(Package, UNiagaraParameterCollection::StaticClass(), *FileName, RF_Public | RF_Standalone));

    NiagaraParameterCollection->SetCompileId(FGuid(AssetData->GetStringField(TEXT("CompileId"))));

    TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;
    const TSharedPtr<FJsonObject>* SourceMaterialCollection;
    
    if (AssetData->TryGetObjectField(TEXT("SourceMaterialCollection"), SourceMaterialCollection))
        LoadObject(SourceMaterialCollection, MaterialParameterCollection);

    NiagaraParameterCollection->SetSourceMaterialCollection(MaterialParameterCollection);

    const TArray<TSharedPtr<FJsonValue>>* ParametersPtr;
    if (AssetData->TryGetArrayField(TEXT("Parameters"), ParametersPtr)) {
        for (const TSharedPtr<FJsonValue> ParameterPtr : *ParametersPtr) {
            TSharedPtr<FJsonObject> ParameterObj = ParameterPtr->AsObject();

            FName Name = FName(*ParameterObj->GetStringField(TEXT("Name")));
        }
    }

    /* Handle edit changes, and add it to the content browser */
    return OnAssetCreation(NiagaraParameterCollection);
}