/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Graph/MaterialGraph.h"

class IMaterialImporter : public IMaterialGraph {
public:
	IMaterialImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		IMaterialGraph(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;
};

REGISTER_IMPORTER(IMaterialImporter, {
	"Material"
}, "Material Assets");