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

TArray<FString> IMaterialGraph::IgnoredExpressions;

IMaterialGraph::IMaterialGraph(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
	IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass)
{
	/* Handled manually by IMaterialGraph */
	IgnoredExpressions = {
		"MaterialExpressionComment",
		"MaterialFunction",
		"Material"
	};
}

TSharedPtr<FJsonObject> IMaterialGraph::FindEditorOnlyData(const FString& Type, const FString& Outer, TMap<FName, FExportData>& OutExports, TArray<FName>& ExpressionNames, bool bFilterByOuter) {
	TSharedPtr<FJsonObject> EditorOnlyData;

	/* Filter array if needed */
	for (const TSharedPtr<FJsonValue> Value : bFilterByOuter ? FilterExportsByOuter(Outer) : AllJsonObjects) {
		TSharedPtr<FJsonObject> Object = TSharedPtr<FJsonObject>(Value->AsObject());

		FString ExportType = Object->GetStringField(TEXT("Type"));
		FString ExportName = Object->GetStringField(TEXT("Name"));

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
		ExpressionNames.Add(FName(ExportName));
		OutExports.Add(FName(ExportName), FExportData(ExportType, Outer, Object));
	}

	return EditorOnlyData;
}

TMap<FName, UMaterialExpression*> IMaterialGraph::ConstructExpressions(UObject* Parent, const FString& Outer, TArray<FName>& ExpressionNames, TMap<FName, FExportData>& Exports) {
	TMap<FName, UMaterialExpression*> CreatedExpressionMap;

	/* Go through each expression name, find the matching expression export, and create the expression */
	for (FName Name : ExpressionNames) {
		FName Type;
		FJsonObject* SharedObjectRef = nullptr;

		for (TTuple<FName, FExportData>& Key : Exports) {
			/* Needs to be shared or else it might get deleted after use */
			const TSharedPtr<FJsonObject>* NewSharedObjectRef = new TSharedPtr<FJsonObject>(Key.Value.Json);

			if (Key.Key == Name && Key.Value.Outer == FName(*Outer)) {
				Type = Key.Value.Type;
				
				SharedObjectRef = NewSharedObjectRef->Get();

				break;
			}
		}

		/* Not found */
		if (SharedObjectRef == nullptr) {
			continue;
		}

		/* Create the expression */
		UMaterialExpression* Expression = CreateEmptyExpression(Parent, Name, Type, SharedObjectRef);

		/* If nullptr, expression isn't valid */
		if (Expression == nullptr) {
			continue;
		}

		CreatedExpressionMap.Add(Name, Expression);
	}

	return CreatedExpressionMap;
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

void IMaterialGraph::PropagateExpressions(UObject* Parent, TArray<FName>& ExpressionNames, TMap<FName, FExportData>& Exports, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, bool bCheckOuter, bool bSubgraph) {
	for (FName ExportName : ExpressionNames) {
		FExportData* Type = Exports.Find(ExportName);

		/* Get variables from the export data */
		FName ExpressionType = Type->Type;
		FJsonObject* ExportJson = Type->Json;
		
		TSharedPtr<FJsonObject> Properties = ExportJson->GetObjectField(TEXT("Properties"));

		/* Find the expression from FName */
		if (!CreatedExpressionMap.Contains(ExportName)) continue;
		UMaterialExpression* Expression = *CreatedExpressionMap.Find(ExportName);

		FString Outer; {
			ExportJson->TryGetStringField(TEXT("Outer"), Outer);
		}

		bool bAddToParentExpression = true;
		
		/* Sub-graph (natively only on Unreal Engine 5) */
		if (Properties->HasField(TEXT("SubgraphExpression"))) {
			TSharedPtr<FJsonObject> SubGraphExpressionObject = Properties->GetObjectField(TEXT("SubgraphExpression"));

			FName SubGraphExpressionName = GetExportNameOfSubobject(SubGraphExpressionObject->GetStringField(TEXT("ObjectName")));

			UMaterialExpression* SubGraphExpression = *CreatedExpressionMap.Find(SubGraphExpressionName);

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
		if (ExpressionType == "MaterialExpressionMaterialFunctionCall") {
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
		if (ExpressionType == "MaterialExpressionQualitySwitch") {
			UMaterialExpressionQualitySwitch* QualitySwitch = Cast<UMaterialExpressionQualitySwitch>(Expression);

			const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
			
			if (ExportJson->TryGetArrayField(TEXT("Inputs"), InputsPtr)) {
				int i = 0;
				for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
					FJsonObject* InputObject = InputValue->AsObject().Get();
					FName InputExpressionName = GetExpressionName(InputObject);
					if (CreatedExpressionMap.Contains(InputExpressionName)) {
						FExpressionInput Input = PopulateExpressionInput(InputObject, *CreatedExpressionMap.Find(InputExpressionName));
						QualitySwitch->Inputs[i] = Input;
					}
					i++;
				}
			}
		} else if (ExpressionType == "MaterialExpressionShadingPathSwitch") {
			UMaterialExpressionShadingPathSwitch* ShadingPathSwitch = Cast<UMaterialExpressionShadingPathSwitch>(Expression);

			const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
			
			if (ExportJson->TryGetArrayField(TEXT("Inputs"), InputsPtr)) {
				int i = 0;
				for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
					FJsonObject* InputObject = InputValue->AsObject().Get();
					FName InputExpressionName = GetExpressionName(InputObject);
					if (CreatedExpressionMap.Contains(InputExpressionName)) {
						FExpressionInput Input = PopulateExpressionInput(InputObject, *CreatedExpressionMap.Find(InputExpressionName));
						ShadingPathSwitch->Inputs[i] = Input;
					}
					i++;
				}
			}
		} else if (ExpressionType == "MaterialExpressionFeatureLevelSwitch") {
			UMaterialExpressionFeatureLevelSwitch* FeatureLevelSwitch = Cast<UMaterialExpressionFeatureLevelSwitch>(Expression);

			const TArray<TSharedPtr<FJsonValue>>* InputsPtr;
			
			if (ExportJson->TryGetArrayField(TEXT("Inputs"), InputsPtr)) {
				int i = 0;
				for (const TSharedPtr<FJsonValue> InputValue : *InputsPtr) {
					FJsonObject* InputObject = InputValue->AsObject().Get();
					FName InputExpressionName = GetExpressionName(InputObject);
					if (CreatedExpressionMap.Contains(InputExpressionName)) {
						FExpressionInput Input = PopulateExpressionInput(InputObject, *CreatedExpressionMap.Find(InputExpressionName));
						FeatureLevelSwitch->Inputs[i] = Input;
					}
					i++;
				}
			}
		}

		MaterialGraphNode_ExpressionWrapper(Parent, Expression, Properties);

		if (bAddToParentExpression) {
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
}

void IMaterialGraph::MaterialGraphNode_AddComment(UObject* Parent, UMaterialExpressionComment* Comment) {
#if ENGINE_MAJOR_VERSION >= 5
	if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) MaterialFunction->GetExpressionCollection().AddComment(Comment);
	if (UMaterial* Material = Cast<UMaterial>(Parent)) Material->GetExpressionCollection().AddComment(Comment);
#else
	if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) MaterialFunction->FunctionEditorComments.Add(Comment);
	if (UMaterial* Material = Cast<UMaterial>(Parent)) Material->EditorComments.Add(Comment);
#endif
}

void IMaterialGraph::MaterialGraphNode_ConstructComments(UObject* Parent, const TSharedPtr<FJsonObject>& Json, TMap<FName, FExportData>& Exports) {
	const TArray<TSharedPtr<FJsonValue>>* StringExpressionComments;

	/* Iterate through comments */
	if (Json->TryGetArrayField(TEXT("EditorComments"), StringExpressionComments)) {
		for (const TSharedPtr<FJsonValue> ExpressionComment : *StringExpressionComments) {
			if (ExpressionComment->IsNull()) {
				continue;
			}

			FName ExportName = GetExportNameOfSubobject(ExpressionComment.Get()->AsObject()->GetStringField(TEXT("ObjectName")));

			/* Get properties of comment, and create it relative to parent */
			const TSharedPtr<FJsonObject> Properties = Exports.Find(ExportName)->Json->GetObjectField(TEXT("Properties"));
			UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Parent, UMaterialExpressionComment::StaticClass(), ExportName, RF_Transactional);

			/* Deserialize and send it off to the material */
			MaterialGraphNode_ExpressionWrapper(Parent, Comment, Properties);
			GetObjectSerializer()->DeserializeObjectProperties(Properties, Comment);

			MaterialGraphNode_AddComment(Parent, Comment);
		}
	}

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
		MaterialGraphNode_AddComment(Parent, Comment);
	}
}

void IMaterialGraph::MaterialGraphNode_ExpressionWrapper(UObject* Parent, UMaterialExpression* Expression, const TSharedPtr<FJsonObject>& Json) {
	if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(Parent)) {
		Expression->Function = MaterialFunction;
	} else if (UMaterial* Material = Cast<UMaterial>(Parent)) {
		Expression->Material = Material;
	}

	if (UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(Expression)) {
		const TSharedPtr<FJsonObject>* TexturePtr;
		
		if (Json->TryGetObjectField(TEXT("Texture"), TexturePtr)) {
#if ENGINE_MAJOR_VERSION >= 5
			LoadObject(TexturePtr, TextureBase->Texture);
#else
				/* For UE4: use a different method of TObjectPtr for the texture */
				TObjectPtr<UTexture> TextureObjectPtr;
				LoadObject(TexturePtr, TextureObjectPtr);
				TextureBase->Texture = TextureObjectPtr.Get();
#endif

			Expression->UpdateParameterGuid(true, false);
		}
	}
}

UMaterialExpression* IMaterialGraph::CreateEmptyExpression(UObject* Parent, const FName Name, FName Type, FJsonObject* LocalizedObject) {
	/* Unhandled expressions */
	if (IgnoredExpressions.Contains(Type.ToString())) {
		return nullptr;
	}

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