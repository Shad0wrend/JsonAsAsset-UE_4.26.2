/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Importer.h"

/*
 * Material Graph Handler
 * Handles everything needed to create a material graph from JSON.
*/
class IMaterialGraph : public IImporter {
public:
	IMaterialGraph(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass);

	/* UMaterialExpression, Properties */
	TMap<FString, FJsonObject*> MissingNodeClasses;

protected:
	static TArray<FString> IgnoredExpressions;

	/* Find Material's Editor Only Data */
	TSharedPtr<FJsonObject> FindEditorOnlyData(const FString& Type, const FString& Outer, TMap<FName, FExportData>& OutExports, TArray<FName>& ExpressionNames, bool bFilterByOuter = true);

	/* Functions to Handle Expressions */
	void MaterialGraphNode_ExpressionWrapper(UObject* Parent, UMaterialExpression* Expression, const TSharedPtr<FJsonObject>& Json);
	void MaterialGraphNode_ConstructComments(UObject* Parent, const TSharedPtr<FJsonObject>& Json, TMap<FName, FExportData>& Exports);

	/* Makes each expression with their class */
	TMap<FName, UMaterialExpression*> ConstructExpressions(UObject* Parent, const FString& Outer, TArray<FName>& ExpressionNames, TMap<FName, FExportData>& Exports);
	UMaterialExpression* CreateEmptyExpression(UObject* Parent, FName Name, FName Type, FJsonObject* LocalizedObject);

	/* Modifies Graph Nodes (copies over properties from FJsonObject) */
	void PropagateExpressions(UObject* Parent, TArray<FName>& ExpressionNames, TMap<FName, FExportData>& Exports, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, bool bCheckOuter = false, bool bSubgraph = false);
	static void MaterialGraphNode_AddComment(UObject* Parent, UMaterialExpressionComment* Comment);
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	/* Functions to Handle Node Connections ~~~~~~~~~~~~ */
	static FExpressionInput PopulateExpressionInput(const FJsonObject* JsonProperties, UMaterialExpression* Expression, const FString& Type = "Default");
	static FExpressionOutput PopulateExpressionOutput(const FJsonObject* JsonProperties);

	static FName GetExpressionName(const FJsonObject* JsonProperties, const FString& OverrideParameterName = "Expression");

	static FExpressionInput CreateExpressionInput(const TSharedPtr<FJsonObject>& JsonProperties, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, const FString& PropertyName);
	static FMaterialAttributesInput CreateMaterialAttributesInput(const TSharedPtr<FJsonObject>& JsonProperties, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, const FString& PropertyName);

	void SpawnMaterialDataMissingNotification() const;
};
