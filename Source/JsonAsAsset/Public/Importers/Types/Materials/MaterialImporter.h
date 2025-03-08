/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Graph/MaterialGraph.h"

#if ENGINE_MAJOR_VERSION == 5
#include "Materials/MaterialExpressionComposite.h"
#endif

class IMaterialImporter : public IMaterialGraph {
public:
	IMaterialImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		IMaterialGraph(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;

#if ENGINE_MAJOR_VERSION == 5
	/*
	 * Subgraph Functions
	 * still not done :[
	*/
	void ComposeExpressionPinBase(UMaterialExpressionPinBase* Pin, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, const TSharedPtr<FJsonObject>& _JsonObject, TMap<FName, FExportData>& Exports);
#endif
	TArray<TSharedPtr<FJsonValue>> FilterGraphNodesBySubgraphExpression(const FString& Outer);
};

REGISTER_IMPORTER(IMaterialImporter, {
	"Material"
});