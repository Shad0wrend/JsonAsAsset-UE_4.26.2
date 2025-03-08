/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Animation/BlendSpaceImporter.h"

bool IBlendSpaceImporter::Import() {
	TSharedPtr<FJsonObject> AssetData = JsonObject->GetObjectField(TEXT("Properties"));
	UBlendSpace* BlendSpace = NewObject<UBlendSpace>(Package, UBlendSpace::StaticClass(), *FileName, RF_Public | RF_Standalone);
	
	BlendSpace->Modify();

	/* Cast to an object class to access variables */
	CBlendSpaceDerived* BlendSpaceDerived = Cast<CBlendSpaceDerived>(BlendSpace);

	auto SampleData = AssetData->GetArrayField(TEXT("SampleData"));

	/* Add samples */
	for (const TSharedPtr<FJsonValue>& JsonObjectValue : SampleData) {
		const TSharedPtr<FJsonObject> JsonObjectVal = JsonObjectValue->AsObject();

		if (JsonObjectVal.IsValid()) {
			auto AnimationJsonObject = JsonObjectVal->GetObjectField(TEXT("Animation"));

			FString AnimationPath = AnimationJsonObject->GetStringField(TEXT("ObjectPath")).Replace(TEXT("FortniteGame/Content"), TEXT("/Game"));
			AnimationPath.Split(".", &AnimationPath, nullptr);

			UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *AnimationPath);

			BlendSpace->Modify();
			BlendSpaceDerived->CreateNewSample(Cast<UAnimSequence>(Object), ObjectToVector(JsonObjectVal->GetObjectField(TEXT("SampleValue")).Get()));
			BlendSpace->PostEditChange();
		}
	}

	/* Ensure internal state is refreshed after adding all samples */
	BlendSpace->ValidateSampleData();
	BlendSpace->MarkPackageDirty();
	BlendSpace->PostEditChange();
	
	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(AssetData,
	{
		"SampleData",
		"GridSamples"
	}), BlendSpace);

	return OnAssetCreation(BlendSpace);
}

void CBlendSpaceDerived::CreateNewSample(UAnimSequence* AnimationSequence, const FVector& SampleValue) {
	SampleData.Add(FBlendSample(AnimationSequence, SampleValue, true, true));
	
	PreviewBasePose = nullptr;
}