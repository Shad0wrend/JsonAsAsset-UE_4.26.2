/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Constructor/TemplatedImporter.h"

/* Explicit instantiation of ITemplatedImporter for UObject */
template class ITemplatedImporter<UObject>;

template <typename AssetType>
bool ITemplatedImporter<AssetType>::Import() {
	try {
		TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));
		GetObjectSerializer()->SetPackageForDeserialization(Package);

		AssetType* Asset = NewObject<AssetType>(Package, AssetClass ? AssetClass : AssetType::StaticClass(), FName(FileName), RF_Public | RF_Standalone);

		Asset->MarkPackageDirty();

		UObjectSerializer* ObjectSerializer = GetObjectSerializer();
		ObjectSerializer->SetPackageForDeserialization(Package);
		ObjectSerializer->SetExportForDeserialization(JsonObject);
		ObjectSerializer->ParentAsset = Asset;

		ObjectSerializer->DeserializeExports(AllJsonObjects);
		
		GetObjectSerializer()->DeserializeObjectProperties(Properties, Asset);

		return OnAssetCreation(Asset);
	} catch (const char* Exception) {
		UE_LOG(LogJson, Error, TEXT("%s"), *FString(Exception));
	}

	return false;
}
