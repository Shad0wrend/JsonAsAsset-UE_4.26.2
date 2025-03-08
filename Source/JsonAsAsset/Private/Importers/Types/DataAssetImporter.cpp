/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/DataAssetImporter.h"
#include "Engine/DataAsset.h"

bool IDataAssetImporter::Import() {
	UDataAsset* DataAsset = NewObject<UDataAsset>(Package, AssetClass, FName(FileName), RF_Public | RF_Standalone);
	DataAsset->MarkPackageDirty();

	UObjectSerializer* ObjectSerializer = GetObjectSerializer();
	ObjectSerializer->SetPackageForDeserialization(Package);
	ObjectSerializer->SetExportForDeserialization(JsonObject);
	ObjectSerializer->ParentAsset = DataAsset;

	ObjectSerializer->DeserializeExports(AllJsonObjects);

	ObjectSerializer->DeserializeObjectProperties(AssetData, DataAsset);
	
	return OnAssetCreation(DataAsset);
}
