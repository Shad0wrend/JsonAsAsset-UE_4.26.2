/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Constructor/Graph/MaterialGraph.h"
#include "Styling/SlateIconFinder.h"

/* Expressions */
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionShadingPathSwitch.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionReroute.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Materials/MaterialExpressionTextureBase.h"
#endif

static TWeakPtr<SNotificationItem> MaterialGraphNotification;

TSharedPtr<FJsonObject> IMaterialGraph::FindEditorOnlyData(const FString& Type, const FString& Outer, FMaterialExpressionNodeExportContainer& Container) {
	TSharedPtr<FJsonObject> EditorOnlyData;

	/* Filter array if needed */
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		TSharedPtr<FJsonObject> Object = TSharedPtr<FJsonObject>(Value->AsObject());

		FString ExportType = Object->GetStringField(TEXT("Type"));
		FName ExportName(Object->GetStringField(TEXT("Name")));

		/* If an editor only data object is found, just set it */
		if (ExportType == Type + "EditorOnlyData") {
			EditorOnlyData = Object;
			continue;
		}

		/* For older versions, the "editor" data is in the main UMaterial/UMaterialFunction export */
		if (ExportType == Type) {
			EditorOnlyData = Object;
			continue;
		}

		/* Add to the list of expressions */
		Container.Expressions.Add(FMaterialExpressionNodeExport(
			ExportName,
			FName(ExportType),
			FName(Outer),
			Object
		));
	}

	return EditorOnlyData->GetObjectField(TEXT("Properties"));
}

void IMaterialGraph::ConstructExpressions(UObject* Parent, FMaterialExpressionNodeExportContainer& Container) {
	/* Go through each expression, and create the expression */
	for (FMaterialExpressionNodeExport& Export : Container.Expressions) {
		TSharedPtr<FJsonObject> ExportJsonObject = Export.JsonObject;

		/* Invalid Json Object */
		if (!JsonObject.IsValid()) {
			continue;
		}

		/* Expression information */
		const FName Name = Export.Name;
		const FName Type = Export.Type;

		UMaterialExpression* Expression = CreateEmptyExpression(Parent, Name, Type, ExportJsonObject);

		/* If nullptr, expression isn't valid */
		if (Expression == nullptr) {
			continue;
		}

		Export.Expression = Expression;
	}
}

void IMaterialGraph::PropagateExpressions(UObject* Parent, FMaterialExpressionNodeExportContainer& Container) {
	for (FMaterialExpressionNodeExport Export : Container.Expressions) {
		/* Get variables from the export data */
		FName Type = Export.Type;

		/* Get Json Objects from Export */
		TSharedPtr<FJsonObject> ExportJsonObject = Export.JsonObject;
		TSharedPtr<FJsonObject> Properties = Export.GetProperties();

		/* Created Expression */
		UMaterialExpression* Expression = Export.Expression;

		/* Skip nulled expressions */
		if (Expression == nullptr) {
			continue;
		}
		
		bool bAddToParentExpression = true;
		
		/* Sub-graph (natively only on Unreal Engine 5) */
		if (Properties->HasField(TEXT("SubgraphExpression"))) {
			TSharedPtr<FJsonObject> SubGraphExpressionObject = Properties->GetObjectField(TEXT("SubgraphExpression"));

			FName SubGraphExpressionName = GetExportNameOfSubobject(SubGraphExpressionObject->GetStringField(TEXT("ObjectName")));

			FMaterialExpressionNodeExport SubGraphExport = Container.Find(SubGraphExpressionName);
			UMaterialExpression* SubGraphExpression = SubGraphExport.Expression;

			/* SubgraphExpression is only on Unreal Engine 5 */
#if ENGINE_MAJOR_VERSION > 4
			Expression->SubgraphExpression = SubGraphExpression;
#else
			/* Add it to the subgraph function ~ UE4 ONLY */

			/* Work in progress
			
			UMaterialFunction* ParentSubgraphFunction = SubgraphFunctions[SubGraphExpressionName];

			Expression = CreateEmptyExpression(ParentSubgraphFunction, ExportName, ExpressionType, ExportJson);

			Expression->Function = ParentSubgraphFunction;
			ParentSubgraphFunction->FunctionExpressions.Add(Expression);

			bAddToParentExpression = false;

			*/
			continue;
#endif
		}

		/* ------------ Manually check for Material Function Calls ------------  */
		if (Type == "MaterialExpressionMaterialFunctionCall") {
			UMaterialExpressionMaterialFunctionCall* MaterialFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

			const TSharedPtr<FJsonObject>* MaterialFunctionPtr;
			
			if (Properties->TryGetObjectField(TEXT("MaterialFunction"), MaterialFunctionPtr)) {
				/* For UE4, we fallback to TWeakObjectPtr */
#if ENGINE_MAJOR_VERSION == 4
				TObjectPtr<UMaterialFunctionInterface> MaterialFunctionObjectPtr;
				MaterialFunctionObjectPtr = MaterialFunctionCall->MaterialFunction;
				
				LoadObject(MaterialFunctionPtr, MaterialFunctionObjectPtr);
#else
				LoadObject(MaterialFunctionPtr, MaterialFunctionCall->MaterialFunction);
#endif
			}
		}

		/* Sets 99% of properties for nodes */
		GetObjectSerializer()->DeserializeObjectProperties(Properties, Expression);

		/* Material Nodes with edited properties (ex: 9 objects with the same name ---> array of objects) */
		if (Type == "MaterialExpressionQualitySwitch") {
			UMaterialExpressionQualitySwitch* QualitySwitch = Cast<UMaterialExpressionQualitySwitch>(Expression);

			const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
			
			if (ExportJsonObject->TryGetArrayField(TEXT("Inputs"), InputsPtr)) {
				int i = 0;
				for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
					FJsonObject* InputObject = InputValue->AsObject().Get();
					FName InputExpressionName = GetExpressionName(InputObject);
					
					if (Container.Contains(InputExpressionName)) {
						FExpressionInput Input = PopulateExpressionInput(InputObject, Container.GetExpressionByName(InputExpressionName));
						QualitySwitch->Inputs[i] = Input;
					}
					i++;
				}
			}
		} else if (Type == "MaterialExpressionShadingPathSwitch") {
			UMaterialExpressionShadingPathSwitch* ShadingPathSwitch = Cast<UMaterialExpressionShadingPathSwitch>(Expression);

			const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
			
			if (ExportJsonObject->TryGetArrayField(TEXT("Inputs"), InputsPtr)) {
				int i = 0;
				for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
					FJsonObject* InputObject = InputValue->AsObject().Get();
					FName InputExpressionName = GetExpressionName(InputObject);
					
					if (Container.Contains(InputExpressionName)) {
						FExpressionInput Input = PopulateExpressionInput(InputObject, Container.GetExpressionByName(InputExpressionName));
						ShadingPathSwitch->Inputs[i] = Input;
					}
					i++;
				}
			}
		} else if (Type == "MaterialExpressionFeatureLevelSwitch") {
			UMaterialExpressionFeatureLevelSwitch* FeatureLevelSwitch = Cast<UMaterialExpressionFeatureLevelSwitch>(Expression);

			const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
			
			if (ExportJsonObject->TryGetArrayField(TEXT("Inputs"), InputsPtr)) {
				int i = 0;
				for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
					FJsonObject* InputObject = InputValue->AsObject().Get();
					FName InputExpressionName = GetExpressionName(InputObject);
					
					if (Container.Contains(InputExpressionName)) {
						FExpressionInput Input = PopulateExpressionInput(InputObject, Container.GetExpressionByName(InputExpressionName));
						FeatureLevelSwitch->Inputs[i] = Input;
					}
					i++;
				}
			}
		}

		SetExpressionParent(Parent, Expression, Properties);

		if (bAddToParentExpression) {
			AddExpressionToParent(Parent, Expression);
		}
	}
}

void IMaterialGraph::CreateExtraNodeInformation(UObject* Parent) {
	/* If there is a missing node in the graph */
	for (TTuple<FString, FJsonObject*>& Key : MissingNodeClasses) {
		const TSharedPtr<FJsonObject>* SharedObject = new TSharedPtr<FJsonObject>(Key.Value);

		const TSharedPtr<FJsonObject> Properties = SharedObject->Get()->GetObjectField(TEXT("Properties"));
		UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Parent, UMaterialExpressionComment::StaticClass(), *("UMaterialExpressionComment_" + Key.Key), RF_Transactional);

		Comment->Text = *("Missing Node Class " + Key.Key);
		Comment->CommentColor = FLinearColor(1.0, 0.0, 0.0);
		Comment->bCommentBubbleVisible = true;
		Comment->SizeX = 415;
		Comment->SizeY = 40;

		Comment->Desc = "A node is missing in your Unreal Engine build, this may be for many reasons, primarily due to your version of Unreal being younger than the one your porting from.";

		GetObjectSerializer()->DeserializeObjectProperties(Properties, Comment);
		AddExpressionToParent(Parent, Comment);
	}
}

void IMaterialGraph::SetExpressionParent(UObject* Parent, UMaterialExpression* Expression, const TSharedPtr<FJsonObject>& Json) {
	if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) {
		Expression->Function = MaterialFunction;
	} else if (UMaterial* Material = Cast<UMaterial>(Parent)) {
		Expression->Material = Material;
	}

	if (Cast<UMaterialExpressionTextureBase>(Expression)) {
		const TSharedPtr<FJsonObject>* TexturePtr;
	
		if (Json->TryGetObjectField(TEXT("Texture"), TexturePtr)) {
			Expression->UpdateParameterGuid(true, false);
		}
	}
}

void IMaterialGraph::AddExpressionToParent(UObject* Parent, UMaterialExpression* Expression)
{
	/* Comments are added differently */
	if (UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expression)) {
		/* In Unreal Engine 5, we have to get the expression collection to add the comment */
#if ENGINE_MAJOR_VERSION >= 5
		if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) MaterialFunction->GetExpressionCollection().AddComment(Comment);
		if (UMaterial* Material = Cast<UMaterial>(Parent)) Material->GetExpressionCollection().AddComment(Comment);
#else
		/* In Unreal Engine 4, we have to get the EditorComments array to add the comment */
		if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) MaterialFunction->FunctionEditorComments.Add(Comment);
		if (UMaterial* Material = Cast<UMaterial>(Parent)) Material->EditorComments.Add(Comment);
#endif
	} else {
		/* Adding expressions is different between UE4 and UE5 */
#if ENGINE_MAJOR_VERSION >= 5
		if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) {
			MaterialFunction->GetExpressionCollection().AddExpression(Expression);
		}

		if (UMaterial* Material = Cast<UMaterial>(Parent)) {
			Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(Expression);
			Expression->UpdateMaterialExpressionGuid(true, false);
			Material->AddExpressionParameter(Expression, Material->EditorParameters);
		}
#else
		if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) {
			MaterialFunction->FunctionExpressions.Add(Expression);
		}

		if (UMaterial* Material = Cast<UMaterial>(Parent)) {
			Material->Expressions.Add(Expression);
			Expression->UpdateMaterialExpressionGuid(true, false);
			Material->AddExpressionParameter(Expression, Material->EditorParameters);
		}
#endif
	}
}

UMaterialExpression* IMaterialGraph::CreateEmptyExpression(UObject* Parent, const FName Name, FName Type, const TSharedPtr<FJsonObject>& LocalizedObject) {
	const UClass* Class = FindObject<UClass>(ANY_PACKAGE, *Type.ToString());

	if (!Class) {
#if ENGINE_MAJOR_VERSION >= 5
		TArray<FString> Redirects = TArray{
			FLinkerLoad::FindNewPathNameForClass("/Script/InterchangeImport." + Type.ToString(), false),
			FLinkerLoad::FindNewPathNameForClass("/Script/Landscape." + Type.ToString(), false)
		};
		
		for (FString RedirectedPath : Redirects) {
			if (!RedirectedPath.IsEmpty() && !Class)
				Class = FindObject<UClass>(nullptr, *RedirectedPath);
		}
#endif

		if (!Class) {
			Class = FindObject<UClass>(ANY_PACKAGE, *Type.ToString().Replace(TEXT("MaterialExpressionPhysicalMaterialOutput"), TEXT("MaterialExpressionLandscapePhysicalMaterialOutput")));
		}
	}

	/* Show missing nodes in graph */
	if (!Class) {
#if ENGINE_MAJOR_VERSION == 4
		/* In Unreal Engine 4, to combat the absence of Sub-graphs, create a Material Function in place of it */
		if (Type == "MaterialExpressionComposite") {
			/*
			Work in progress
			const FString SubgraphFunctionName = FileName + "_" + Name.ToString().Replace(TEXT("MaterialExpression"), TEXT(""));

			const UPackage* ParentPackage = Parent->GetOutermost();
			FString SubgraphFunctionPath = ParentPackage->GetPathName();

			SubgraphFunctionPath.Split("/", &SubgraphFunctionPath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			SubgraphFunctionPath = SubgraphFunctionPath + "/Subgraph/";
			
			UPackage* SubgraphLocalOutermostPkg;
			UPackage* SubgraphLocalPackage = FAssetUtilities::CreateAssetPackage(SubgraphFunctionName, SubgraphFunctionPath, SubgraphLocalOutermostPkg);

			UMaterialFunctionFactoryNew* SubgraphMaterialFunctionFactory = NewObject<UMaterialFunctionFactoryNew>();
			UMaterialFunction* SubgraphMaterialFunction = Cast<UMaterialFunction>(SubgraphMaterialFunctionFactory->FactoryCreateNew(UMaterialFunction::StaticClass(), SubgraphLocalOutermostPkg, *SubgraphFunctionName, RF_Standalone | RF_Public, nullptr, GWarn));

			HandleAssetCreation(SubgraphMaterialFunction);

			SubgraphFunctions.Add(Name, SubgraphMaterialFunction);

			return CreateEmptyExpression(Parent, Name, "MaterialExpressionMaterialFunctionCall", LocalizedObject);

			*/
		}
#endif
		
		TSharedPtr<FJsonObject>* SharedObject = new TSharedPtr<FJsonObject>(LocalizedObject);
		MissingNodeClasses.Add(Type.ToString(), SharedObject->Get());

		GLog->Log(*("JsonAsAsset: Missing Node " + Type.ToString() + " in Parent " + Parent->GetName()));
		FNotificationInfo Info = FNotificationInfo(FText::FromString("Missing Node (" + Parent->GetName() + ")"));

		Info.bUseLargeFont = false;
		Info.FadeOutDuration = 2.5f;
		Info.ExpireDuration = 8.0f;
		Info.WidthOverride = FOptionalSize(456);
		Info.bUseThrobber = false;

		SetNotificationSubText(Info, FText::FromString(Type.ToString()));

#pragma warning(disable: 4800)
		const UClass* MaterialClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.Material"));
		Info.Image = FSlateIconFinder::FindCustomIconBrushForClass(MaterialClass, TEXT("ClassThumbnail"));

		MaterialGraphNotification = FSlateNotificationManager::Get().AddNotification(Info);
		MaterialGraphNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

		return NewObject<UMaterialExpression>(
			Parent,
			UMaterialExpressionReroute::StaticClass(),
			Name
		);
	}

	return NewObject<UMaterialExpression>
	(
		Parent,
		Class, /* Find class using ANY_PACKAGE (may error in the future) */
		Name,
		RF_Transactional
	);
}

FExpressionInput IMaterialGraph::PopulateExpressionInput(const FJsonObject* JsonProperties, UMaterialExpression* Expression, const FString& Type) {
	FExpressionInput Input;
	Input.Expression = Expression;

	/* Each Mask input/output */
	int OutputIndex;
	if (JsonProperties->TryGetNumberField(TEXT("OutputIndex"), OutputIndex)) Input.OutputIndex = OutputIndex;
	FString InputName;
	if (JsonProperties->TryGetStringField(TEXT("InputName"), InputName)) Input.InputName = FName(InputName);
	int Mask;
	if (JsonProperties->TryGetNumberField(TEXT("Mask"), Mask)) Input.Mask = Mask;
	int MaskR;
	if (JsonProperties->TryGetNumberField(TEXT("MaskR"), MaskR)) Input.MaskR = MaskR;
	int MaskG;
	if (JsonProperties->TryGetNumberField(TEXT("MaskG"), MaskG)) Input.MaskG = MaskG;
	int MaskB;
	if (JsonProperties->TryGetNumberField(TEXT("MaskB"), MaskB)) Input.MaskB = MaskB;
	int MaskA;
	if (JsonProperties->TryGetNumberField(TEXT("MaskA"), MaskA)) Input.MaskA = MaskA;

	if (Type == "Color") {
		if (FColorMaterialInput* ColorInput = reinterpret_cast<FColorMaterialInput*>(&Input)) {
			bool UseConstant;
			if (JsonProperties->TryGetBoolField(TEXT("UseConstant"), UseConstant)) ColorInput->UseConstant = UseConstant;
			const TSharedPtr<FJsonObject>* Constant;
			if (JsonProperties->TryGetObjectField(TEXT("Constant"), Constant)) ColorInput->Constant = ObjectToLinearColor(Constant->Get()).ToFColor(true);
			Input = FExpressionInput(*ColorInput);
		}
	} else if (Type == "Scalar") {
		if (FScalarMaterialInput* ScalarInput = reinterpret_cast<FScalarMaterialInput*>(&Input)) {
			bool UseConstant;
			if (JsonProperties->TryGetBoolField(TEXT("UseConstant"), UseConstant)) ScalarInput->UseConstant = UseConstant;
#if ENGINE_MAJOR_VERSION >= 5
			float Constant;
#else
			double Constant;
#endif
			if (JsonProperties->TryGetNumberField(TEXT("Constant"), Constant)) ScalarInput->Constant = Constant;
			Input = FExpressionInput(*ScalarInput);
		}
	} else if (Type == "Vector") {
		if (FVectorMaterialInput* VectorInput = reinterpret_cast<FVectorMaterialInput*>(&Input)) {
			bool UseConstant;
			if (JsonProperties->TryGetBoolField(TEXT("UseConstant"), UseConstant)) VectorInput->UseConstant = UseConstant;
			const TSharedPtr<FJsonObject>* Constant;
			if (JsonProperties->TryGetObjectField(TEXT("Constant"), Constant)) VectorInput->Constant = ObjectToVector3f(Constant->Get());
			Input = FExpressionInput(*VectorInput);
		}
	}

	return Input;
}

FName IMaterialGraph::GetExpressionName(const FJsonObject* JsonProperties, const FString& OverrideParameterName) {
	const TSharedPtr<FJsonValue> ExpressionField = JsonProperties->TryGetField(OverrideParameterName);

	if (ExpressionField == nullptr || ExpressionField->IsNull()) {
		/* Must be from < 4.25 */
		return FName(JsonProperties->GetStringField(TEXT("ExpressionName")));
	}

	const TSharedPtr<FJsonObject> ExpressionObject = ExpressionField->AsObject();
	FString ObjectName;
	
	if (ExpressionObject->TryGetStringField(TEXT("ObjectName"), ObjectName)) {
		return GetExportNameOfSubobject(ObjectName);
	}

	return NAME_None;
}

void IMaterialGraph::SpawnMaterialDataMissingNotification() const {
	FNotificationInfo Info = FNotificationInfo(FText::FromString("Material Data Missing (" + FileName + ")"));
	Info.ExpireDuration = 7.0f;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = true;
	Info.WidthOverride = FOptionalSize(350);
	SetNotificationSubText(Info, FText::FromString(FString("Please use the correct FModel provided in the JsonAsAsset server.")));

	const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
}