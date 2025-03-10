/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Importer.h"

/* A structure to hold data for a material expression node. */
struct FMaterialExpressionNodeExport {
	FMaterialExpressionNodeExport(): Expression(nullptr) {};

	FName Name;
	FName Type;
	FName Outer;

	/* The json object of the expression, this is not Properties */
	TSharedPtr<FJsonObject> JsonObject;

	/* Expression created */
	UMaterialExpression* Expression;

	FMaterialExpressionNodeExport(const FName& Name, const FName& Type, const FName Outer, const TSharedPtr<FJsonObject>& JsonObject)
		: Name(Name), Type(Type), Outer(Outer), JsonObject(JsonObject), Expression(nullptr) {
	}

	TSharedPtr<FJsonObject> GetProperties() const {
		TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

		return Properties;
	}
};

struct FMaterialExpressionNodeExportContainer {
	/* Array of Expression Exports */
	TArray<FMaterialExpressionNodeExport> Expressions;
	
	FMaterialExpressionNodeExportContainer() {};

	FMaterialExpressionNodeExport Find(const FName Name) {
		for (FMaterialExpressionNodeExport Export : Expressions) {
			if (Export.Name == Name) {
				return Export;
			}
		}

		return FMaterialExpressionNodeExport();
	}

	UMaterialExpression* GetExpressionByName(const FName Name) {
		for (FMaterialExpressionNodeExport Export : Expressions) {
			if (Export.Name == Name) {
				return Export.Expression;
			}
		}

		return nullptr;
	}
	
	bool Contains(const FName Name) {
		for (FMaterialExpressionNodeExport Export : Expressions) {
			if (Export.Name == Name) {
				return true;
			}
		}

		return false;
	}
	
	int Num() const {
		return Expressions.Num();
	}
};


/*
 * Material Graph Handler
 * Handles everything needed to create a material graph from JSON.
*/
class IMaterialGraph : public IImporter {
public:
	IMaterialGraph(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}
	
	/* UMaterialExpression, Properties */
	TMap<FString, FJsonObject*> MissingNodeClasses;

protected:
	/* Find Material's Editor Only Data */
	TSharedPtr<FJsonObject> FindEditorOnlyData(const FString& Type, const FString& Outer, FMaterialExpressionNodeExportContainer& Container);

	/* Functions to Handle Expressions */
	static void SetExpressionParent(UObject* Parent, UMaterialExpression* Expression, const TSharedPtr<FJsonObject>& Json);
	static void AddExpressionToParent(UObject* Parent, UMaterialExpression* Expression);
	
	void CreateExtraNodeInformation(UObject* Parent);

	/* Makes each expression with their class */
	void ConstructExpressions(UObject* Parent, FMaterialExpressionNodeExportContainer& Container);
	
	UMaterialExpression* CreateEmptyExpression(UObject* Parent, FName Name, FName Type, const TSharedPtr<FJsonObject>& LocalizedObject);

	/* Modifies Graph Nodes (copies over properties from FJsonObject) */
	void PropagateExpressions(UObject* Parent, FMaterialExpressionNodeExportContainer& Container);
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	/* Functions to Handle Node Connections ~~~~~~~~~~~~ */
	static FExpressionInput PopulateExpressionInput(const FJsonObject* JsonProperties, UMaterialExpression* Expression, const FString& Type = "Default");

	static FName GetExpressionName(const FJsonObject* JsonProperties, const FString& OverrideParameterName = "Expression");

	void SpawnMaterialDataMissingNotification() const;

#if ENGINE_MAJOR_VERSION == 4
	/*
	 * In Unreal Engine 4, to combat the absence of Sub-graphs, create a Material Function in place of it
	 * This holds a mapping to the name of the composite node it was created from, and the material
	 * function created in-place of it
	 */

	TMap<FName, UMaterialFunction*> SubgraphFunctions;
#endif
};
