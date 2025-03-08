/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Materials/MaterialImporter.h"

/* Include Material.h (depends on UE Version) */
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3) || ENGINE_MAJOR_VERSION == 4
#include "Materials/Material.h"
#else
#include "MaterialDomain.h"
#endif

#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "Factories/MaterialFactoryNew.h"

#include "Settings/JsonAsAssetSettings.h"

#if ENGINE_MAJOR_VERSION >= 5
#include <Editor/UnrealEd/Classes/MaterialGraph/MaterialGraphNode_Composite.h>
#include <Editor/UnrealEd/Classes/MaterialGraph/MaterialGraphSchema.h>
#include "Editor/MaterialEditor/Private/MaterialEditor.h"
#include "MaterialGraph/MaterialGraph.h"

void IMaterialImporter::ComposeExpressionPinBase(UMaterialExpressionPinBase* Pin, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, const TSharedPtr<FJsonObject>& _JsonObject, TMap<FName, FExportData>& Exports) {
	FJsonObject* Expression = (Exports.Find(GetExportNameOfSubobject(_JsonObject->GetStringField(TEXT("ObjectName"))))->Json)->GetObjectField(TEXT("Properties")).Get();

	Pin->GraphNode->NodePosX = Expression->GetNumberField(TEXT("MaterialExpressionEditorX"));
	Pin->GraphNode->NodePosY = Expression->GetNumberField(TEXT("MaterialExpressionEditorY"));
	Pin->MaterialExpressionEditorX = Expression->GetNumberField(TEXT("MaterialExpressionEditorX"));
	Pin->MaterialExpressionEditorY = Expression->GetNumberField(TEXT("MaterialExpressionEditorY"));

	FString MaterialExpressionGuid;
	if (Expression->TryGetStringField(TEXT("MaterialExpressionGuid"), MaterialExpressionGuid)) Pin->MaterialExpressionGuid = FGuid(MaterialExpressionGuid);

	const TArray<TSharedPtr<FJsonValue>>* ReroutePins;
	if (Expression->TryGetArrayField(TEXT("ReroutePins"), ReroutePins)) {
		for (const TSharedPtr<FJsonValue> ReroutePin : *ReroutePins) {
			if (ReroutePin->IsNull()) continue;
			TSharedPtr<FJsonObject> ReroutePinObject = ReroutePin->AsObject();
			TSharedPtr<FJsonObject> RerouteObj = GetExportByObjectPath(ReroutePinObject->GetObjectField(TEXT("Expression")))->AsObject();
			UMaterialExpressionReroute* RerouteExpression = Cast<UMaterialExpressionReroute>(*CreatedExpressionMap.Find(FName(RerouteObj->GetStringField(TEXT("Name")))));

			/* Add reroute to pin */
			Pin->ReroutePins.Add(FCompositeReroute(FName(ReroutePinObject->GetStringField(TEXT("Name"))), RerouteExpression));
		}
	}

	/* Set Pin Direction */
	FString PinDirection;
	if (Expression->TryGetStringField(TEXT("PinDirection"), PinDirection)) {
		EEdGraphPinDirection Enum_PinDirection = EGPD_Input;

		if (PinDirection.EndsWith("EGPD_Input")) Enum_PinDirection = EGPD_Input;
		else if (PinDirection.EndsWith("EGPD_Output")) Enum_PinDirection = EGPD_Output;

		Pin->PinDirection = Enum_PinDirection;
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsPtr;
	if (Expression->TryGetArrayField(TEXT("Outputs"), OutputsPtr)) {
		TArray<FExpressionOutput> Outputs;
		for (const TSharedPtr<FJsonValue> OutputValue : *OutputsPtr) {
			TSharedPtr<FJsonObject> OutputObject = OutputValue->AsObject();
			Outputs.Add(PopulateExpressionOutput(OutputObject.Get()));
		}

		Pin->Outputs = Outputs;
	}

	Pin->Modify();
}
#endif

bool IMaterialImporter::Import() {
	/* Create Material Factory (factory automatically creates the Material) */
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UMaterial* Material = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

	Material->GetReferencedTextures();

	/* Clear any default expressions the engine adds */
#if ENGINE_MAJOR_VERSION >= 5
	Material->GetExpressionCollection().Empty();
#else
	Material->Expressions.Empty();
#endif

	/* Define editor only data from the JSON */
	TMap<FName, FExportData> Exports;
	TArray<FName> ExpressionNames;
	TSharedPtr<FJsonObject> EdProps = FindEditorOnlyData(JsonObject->GetStringField(TEXT("Type")), Material->GetName(), Exports, ExpressionNames, false)->GetObjectField(TEXT("Properties"));
	const TSharedPtr<FJsonObject> StringExpressionCollection = EdProps->GetObjectField(TEXT("ExpressionCollection"));

	/* Map out each expression for easier access */
	TMap<FName, UMaterialExpression*> CreatedExpressionMap = ConstructExpressions(Material, Material->GetName(), ExpressionNames, Exports);
	const UJsonAsAssetSettings* Settings = GetDefault<UJsonAsAssetSettings>();

	/* Missing Material Data */
	if (Exports.Num() == 0) {
		SpawnMaterialDataMissingNotification();

		return false;
	}

	/* Iterate through all the expression names */
	PropagateExpressions(Material, ExpressionNames, Exports, CreatedExpressionMap, true);
	MaterialGraphNode_ConstructComments(Material, StringExpressionCollection, Exports);

	if (!Settings->AssetSettings.MaterialImportSettings.bSkipResultNodeConnection) {
		TArray<FString> IgnoredProperties = TArray<FString> {
			"ParameterGroupData",
			"ExpressionCollection",
			"CustomizedUVs"
		};

		const TSharedPtr<FJsonObject> RawConnectionData = TSharedPtr<FJsonObject>(EdProps);
		for (FString Property : IgnoredProperties) {
			if (RawConnectionData->HasField(Property))
				RawConnectionData->RemoveField(Property);
		}

		/* Connect all pins using deserializer */
#if ENGINE_MAJOR_VERSION >= 5
		GetObjectSerializer()->DeserializeObjectProperties(RawConnectionData, Material->GetEditorOnlyData());
#else
		GetObjectSerializer()->DeserializeObjectProperties(RawConnectionData, Material);
#endif

		/* CustomizedUVs defined here */
		const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
		
		if (EdProps->TryGetArrayField(TEXT("CustomizedUVs"), InputsPtr)) {
			int i = 0;
			for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
				FJsonObject* InputObject = InputValue->AsObject().Get();
				FName InputExpressionName = GetExpressionName(InputObject);

				if (CreatedExpressionMap.Contains(InputExpressionName)) {
					FExpressionInput Input = PopulateExpressionInput(InputObject, *CreatedExpressionMap.Find(InputExpressionName));
#if ENGINE_MAJOR_VERSION >= 5
					Material->GetEditorOnlyData()->CustomizedUVs[i] = *reinterpret_cast<FVector2MaterialInput*>(&Input);
#else
					Material->CustomizedUVs[i] = *reinterpret_cast<FVector2MaterialInput*>(&Input);
#endif
				}
				i++;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* StringParameterGroupData;
	if (EdProps->TryGetArrayField(TEXT("ParameterGroupData"), StringParameterGroupData)) {
		TArray<FParameterGroupData> ParameterGroupData;

		for (const TSharedPtr<FJsonValue> ParameterGroupDataObject : *StringParameterGroupData) {
			if (ParameterGroupDataObject->IsNull()) continue;
			FParameterGroupData GroupData;

			FString GroupName;
			if (ParameterGroupDataObject->AsObject()->TryGetStringField(TEXT("GroupName"), GroupName)) GroupData.GroupName = GroupName;
			int GroupSortPriority;
			if (ParameterGroupDataObject->AsObject()->TryGetNumberField(TEXT("GroupSortPriority"), GroupSortPriority)) GroupData.GroupSortPriority = GroupSortPriority;

			ParameterGroupData.Add(GroupData);
		}

#if ENGINE_MAJOR_VERSION >= 5
		Material->GetEditorOnlyData()->ParameterGroupData = ParameterGroupData;
#else
		Material->ParameterGroupData = ParameterGroupData;
#endif
	}

	/* Handle edit changes, and add it to the content browser */
	if (!HandleAssetCreation(Material)) return false;

#if ENGINE_MAJOR_VERSION >= 5
	bool bEditorGraphOpen = false;
	FMaterialEditor* AssetEditorInstance = nullptr;

	/* Handle Material Graphs */
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		TSharedPtr<FJsonObject> Object = TSharedPtr<FJsonObject>(Value->AsObject());

		FString ExType = Object->GetStringField(TEXT("Type"));
		FString Name = Object->GetStringField(TEXT("Name"));

		if (ExType == "MaterialGraph" && Name != "MaterialGraph_0") {
			TSharedPtr<FJsonObject> GraphProperties = Object->GetObjectField(TEXT("Properties"));
			TSharedPtr<FJsonObject> SubgraphExpression;

			FString SubgraphExpressionName;

			if (!bEditorGraphOpen) {
				/* Create Editor */
				UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
				AssetEditorInstance = reinterpret_cast<FMaterialEditor*>(AssetEditorSubsystem->OpenEditorForAsset(Material) ? AssetEditorSubsystem->FindEditorForAsset(Material, true) : nullptr);

				bEditorGraphOpen = true;
			}

			/* Set SubgraphExpression */
			const TSharedPtr<FJsonObject>* SubgraphExpressionPtr = nullptr;
			if (GraphProperties->TryGetObjectField(TEXT("SubgraphExpression"), SubgraphExpressionPtr) && SubgraphExpressionPtr != nullptr) {
				FJsonObject* SubgraphExpressionObject = SubgraphExpressionPtr->Get();
				FName ExportName = GetExportNameOfSubobject(SubgraphExpressionObject->GetStringField(TEXT("ObjectName")));

				SubgraphExpressionName = ExportName.ToString();
				FExportData Export = *Exports.Find(ExportName);
				SubgraphExpression = Export.Json->GetObjectField(TEXT("Properties"));
			}

			/* Find Material Graph */
			UMaterialGraph* MaterialGraph = AssetEditorInstance->Material->MaterialGraph;
			if (MaterialGraph == nullptr) {
				UE_LOG(LogJson, Log, TEXT("The material graph is not valid!"));
			}

			MaterialGraph->Modify();

			/* Create the composite node that will serve as the gateway into the subgraph */
			UMaterialGraphNode_Composite* GatewayNode = nullptr;
			{
				GatewayNode = Cast<UMaterialGraphNode_Composite>(FMaterialGraphSchemaAction_NewComposite::SpawnNode(MaterialGraph, FVector2D(SubgraphExpression->GetNumberField(TEXT("MaterialExpressionEditorX")), SubgraphExpression->GetNumberField(TEXT("MaterialExpressionEditorY")))));
				GatewayNode->bCanRenameNode = true;
				check(GatewayNode);
			}

			UMaterialGraph* DestinationGraph = Cast<UMaterialGraph>(GatewayNode->BoundGraph);
			UMaterialExpressionComposite* CompositeExpression = CastChecked<UMaterialExpressionComposite>(GatewayNode->MaterialExpression);
			{
				CompositeExpression->Material = Material;
				CompositeExpression->SubgraphName = Name;

				MaterialGraphNode_ExpressionWrapper(Material, CompositeExpression, SubgraphExpression);
			}

			/* Create notification */
			FNotificationInfo Info = FNotificationInfo(FText::FromString("Material Graph imported incomplete"));
			Info.ExpireDuration = 2.0f;
			Info.bUseLargeFont = true;
			Info.bUseSuccessFailIcons = true;
			Info.WidthOverride = FOptionalSize(350);
			Info.SubText = FText::FromString(FString("Material"));

			/* Set icon as successful */
			TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
			NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);

			DestinationGraph->Rename(*CompositeExpression->SubgraphName);
			DestinationGraph->Material = MaterialGraph->Material;

			/* Add Sub-Graph Nodes */
			{
				TArray<TSharedPtr<FJsonValue>> MaterialGraphNodes = FilterGraphNodesBySubgraphExpression(SubgraphExpressionName);
				TMap<FName, FExportData> SubGraphExports;
				TMap<FName, UMaterialExpression*> SubgraphExpressionMapping;
				TArray<FName> SubGraphExpressionNames;

				/* Go through each expression */
				for (const TSharedPtr<FJsonValue> _GraphNode : MaterialGraphNodes) {
					const TSharedPtr<FJsonObject> MaterialGraphObject = TSharedPtr(_GraphNode->AsObject());

					FString GraphNode_Type = MaterialGraphObject->GetStringField(TEXT("Type"));
					FString GraphNode_Name = MaterialGraphObject->GetStringField(TEXT("Name"));

					FName GraphNodeNameName = FName(GraphNode_Name);

					TSharedPtr<FJsonObject>* SharedGraphObject = new TSharedPtr<FJsonObject>(MaterialGraphObject);

					UMaterialExpression* Ex = CreateEmptyExpression(MaterialGraph->Material, GraphNodeNameName, FName(GraphNode_Type), SharedGraphObject->Get());
					if (Ex == nullptr)
						continue;

					Ex->SubgraphExpression = CompositeExpression;
					Ex->Material = MaterialGraph->Material;

					SubGraphExpressionNames.Add(GraphNodeNameName);
					SubGraphExports.Add(GraphNodeNameName, FExportData(GraphNode_Type, MaterialGraph->Material->GetName(), MaterialGraphObject));
					SubgraphExpressionMapping.Add(GraphNodeNameName, Ex);
				}

				/* Setup Input/Output Expressions */
				{
					const TSharedPtr<FJsonObject>* InputExpressions;
					if (SubgraphExpression->TryGetObjectField(TEXT("InputExpressions"), InputExpressions)) {
						TSharedPtr<FJsonObject> InputExpression = TSharedPtr<FJsonObject>(InputExpressions->Get());

						ComposeExpressionPinBase(CompositeExpression->InputExpressions, CreatedExpressionMap, InputExpression, Exports);
					}

					const TSharedPtr<FJsonObject>* OutputExpressions;
					if (SubgraphExpression->TryGetObjectField(TEXT("OutputExpressions"), OutputExpressions)) {
						TSharedPtr<FJsonObject> OutputExpression = TSharedPtr<FJsonObject>(OutputExpressions->Get());

						ComposeExpressionPinBase(CompositeExpression->OutputExpressions, CreatedExpressionMap, OutputExpression, Exports);
					}
				}

				/* Add all the expression properties */
				PropagateExpressions(MaterialGraph->Material, SubGraphExpressionNames, Exports, SubgraphExpressionMapping, true);

				/*
				 * All expressions (hopefully) have their properties
				 * so now we just make a material graph node for each
				 */
				for (const TPair<FName, UMaterialExpression*>& pair : SubgraphExpressionMapping) {
					UMaterialExpression* Expression = pair.Value;
					UMaterialGraphNode* NewNode = DestinationGraph->AddExpression(Expression, false);

					const FGuid NewGuid = FGuid::NewGuid();
					NewNode->NodeGuid = NewGuid;

					NewNode->NodePosX = Expression->MaterialExpressionEditorX;
					NewNode->NodePosY = Expression->MaterialExpressionEditorY;
					NewNode->MaterialExpression = Expression;

					DestinationGraph->AddNode(NewNode);
					NewNode->ReconstructNode();
				}
			}

			CompositeExpression->InputExpressions->Material = MaterialGraph->Material;
			CompositeExpression->OutputExpressions->Material = MaterialGraph->Material;

			GatewayNode->ReconstructNode();
			Cast<UMaterialGraphNode>(CompositeExpression->InputExpressions->GraphNode)->ReconstructNode();
			Cast<UMaterialGraphNode>(CompositeExpression->OutputExpressions->GraphNode)->ReconstructNode();

			DestinationGraph->RebuildGraph();
			DestinationGraph->LinkMaterialExpressionsFromGraph();

			/* Update Original Material */
			AssetEditorInstance->UpdateMaterialAfterGraphChange();
		}
	}
#endif

	const TSharedPtr<FJsonObject>* ShadingModelsPtr;
	
	if (AssetData->TryGetObjectField(TEXT("ShadingModels"), ShadingModelsPtr)) {
		int ShadingModelField;
		
		if (ShadingModelsPtr->Get()->TryGetNumberField(TEXT("ShadingModelField"), ShadingModelField)) {
#if ENGINE_MAJOR_VERSION >= 5
			Material->GetShadingModels().SetShadingModelField(ShadingModelField);
#else
			/* Not to sure what to do in UE4, no function exists to override it. */
#endif
		}
	}

	TSharedPtr<FJsonObject> SerializerProperties = TSharedPtr<FJsonObject>(AssetData);
	GetObjectSerializer()->DeserializeObjectProperties(SerializerProperties, Material);

	Material->UpdateCachedExpressionData();
	
	FMaterialUpdateContext MaterialUpdateContext;
	MaterialUpdateContext.AddMaterial(Material);
	
	Material->ForceRecompileForRendering();

	Material->PostEditChange();
	Material->MarkPackageDirty();
	Material->PreEditChange(nullptr);

	SavePackage();

	return true;
}

/*
 * Filter out Material Graph Nodes
 * by checking their subgraph expression (composite)
 */
TArray<TSharedPtr<FJsonValue>> IMaterialImporter::FilterGraphNodesBySubgraphExpression(const FString& Outer) {
	TArray<TSharedPtr<FJsonValue>> ReturnValue = TArray<TSharedPtr<FJsonValue>>();

	/*
	* How this works:
	* 
	* 1. Find a Material Graph Node
	* 2. Get the Material Expression
	* 3. Compare SubgraphExpression to the one provided
	*    to the function
	*/
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = TSharedPtr<FJsonObject>(Value->AsObject());
		const TSharedPtr<FJsonObject> Properties = TSharedPtr<FJsonObject>(ValueObject->GetObjectField(TEXT("Properties")));

		const TSharedPtr<FJsonObject>* MaterialExpression;
		if (Properties->TryGetObjectField(TEXT("MaterialExpression"), MaterialExpression)) {
			TSharedPtr<FJsonValue> ExpValue = GetExportByObjectPath(*MaterialExpression);
			TSharedPtr<FJsonObject> Expression = TSharedPtr<FJsonObject>(ExpValue->AsObject());
			const TSharedPtr<FJsonObject> _Properties = TSharedPtr<FJsonObject>(Expression->GetObjectField(TEXT("Properties")));

			const TSharedPtr<FJsonObject>* _SubgraphExpression;
			if (_Properties->TryGetObjectField(TEXT("SubgraphExpression"), _SubgraphExpression)) {
				if (Outer == GetExportNameOfSubobject(_SubgraphExpression->Get()->GetStringField(TEXT("ObjectName"))).ToString()) {
					ReturnValue.Add(ExpValue);
				}
			}
		}
	}

	return ReturnValue;
}