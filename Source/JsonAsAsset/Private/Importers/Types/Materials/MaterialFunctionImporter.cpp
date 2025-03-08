/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Materials/MaterialFunctionImporter.h"
#include "Factories/MaterialFunctionFactoryNew.h"

bool IMaterialFunctionImporter::Import() {
	/* Create Material Function Factory (factory automatically creates the MF) */
	UMaterialFunctionFactoryNew* MaterialFunctionFactory = NewObject<UMaterialFunctionFactoryNew>();
	UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(MaterialFunctionFactory->FactoryCreateNew(UMaterialFunction::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

#if ENGINE_MAJOR_VERSION >= 5
	MaterialFunction->GetExpressionCollection().Empty();
#else
	MaterialFunction->FunctionExpressions.Empty();
#endif

	/* Handle edit changes, and add it to the content browser */
	if (!HandleAssetCreation(MaterialFunction)) return false;

	MaterialFunction->StateId = FGuid(AssetData->GetStringField(TEXT("StateId")));
	
	/* Misc properties */
	bool bPrefixParameterNames;
	FString Description;
	bool bExposeToLibrary;
	
	if (AssetData->TryGetStringField(TEXT("Description"), Description)) MaterialFunction->Description = Description;
	if (AssetData->TryGetBoolField(TEXT("bExposeToLibrary"), bExposeToLibrary)) MaterialFunction->bExposeToLibrary = bExposeToLibrary;
	if (AssetData->TryGetBoolField(TEXT("bPrefixParameterNames"), bPrefixParameterNames)) MaterialFunction->bPrefixParameterNames = bPrefixParameterNames;

	/* Define editor only data from the JSON */
	TMap<FName, FExportData> Exports;
	TArray<FName> ExpressionNames;
	const TSharedPtr<FJsonObject> EdProps = FindEditorOnlyData(JsonObject->GetStringField(TEXT("Type")), MaterialFunction->GetName(), Exports, ExpressionNames, false)->GetObjectField(TEXT("Properties"));
	const TSharedPtr<FJsonObject> StringExpressionCollection = EdProps->GetObjectField(TEXT("ExpressionCollection"));

	/* Map out each expression for easier access */
	TMap<FName, UMaterialExpression*> CreatedExpressionMap = ConstructExpressions(MaterialFunction, MaterialFunction->GetName(), ExpressionNames, Exports);

	/* If Missing Material Data */
	if (Exports.Num() == 0) {
		SpawnMaterialDataMissingNotification();

		return false;
	}

	/* Iterate through all the expression names */
	PropagateExpressions(MaterialFunction, ExpressionNames, Exports, CreatedExpressionMap);

	/* Create comments */
	MaterialGraphNode_ConstructComments(MaterialFunction, StringExpressionCollection, Exports);

	MaterialFunction->PreEditChange(nullptr);
	MaterialFunction->PostEditChange();

	SavePackage();

	return true;
}
