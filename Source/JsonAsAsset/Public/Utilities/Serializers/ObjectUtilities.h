/* Copyright JsonAsAsset Contributors 2024-2025 */

#pragma once

#include "UObject/Object.h"
#include "Json.h"
#include "ObjectUtilities.generated.h"

class UPropertySerializer;

UCLASS()
class JSONASASSET_API UObjectSerializer : public UObject {
    GENERATED_BODY()
public:
    UObjectSerializer();

    void SetPropertySerializer(UPropertySerializer* NewPropertySerializer);
    void SetupExports(const TArray<TSharedPtr<FJsonValue>>& InObjects);

    FORCEINLINE UPropertySerializer* GetPropertySerializer() const { return PropertySerializer; }

    void DeserializeObjectProperties(const TSharedPtr<FJsonObject>& Properties, UObject* Object) const;

    void SetExportForDeserialization(const TSharedPtr<FJsonObject>& Object);
    void DeserializeExports(TArray<TSharedPtr<FJsonValue>> InExports);

    UPROPERTY()
    UObject* ParentAsset;
    
    UPROPERTY()
    UPropertySerializer* PropertySerializer;

    TArray<TSharedPtr<FJsonValue>> Exports;

    UPROPERTY()
    TArray<FString> ExportsToNotDeserialize;
};