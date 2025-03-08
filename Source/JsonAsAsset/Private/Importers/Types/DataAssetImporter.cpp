/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/DataAssetImporter.h"
#include "Engine/DataAsset.h"

bool IDataAssetImporter::Import() {
	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

	UDataAsset* DataAsset = NewObject<UDataAsset>(Package, DataAssetClass, FName(FileName), RF_Public | RF_Standalone);
	DataAsset->MarkPackageDirty();

	UObjectSerializer* ObjectSerializer = GetObjectSerializer();
	ObjectSerializer->SetPackageForDeserialization(Package);
	ObjectSerializer->SetExportForDeserialization(JsonObject);
	ObjectSerializer->ParentAsset = DataAsset;

	ObjectSerializer->DeserializeExports(AllJsonObjects);

	ObjectSerializer->DeserializeObjectProperties(Properties, DataAsset);
	
	return OnAssetCreation(DataAsset);
}
