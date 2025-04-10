/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Importers/Types/Animation/PoseAssetImporter.h"
#include "Animation/PoseAsset.h"

bool IPoseAssetImporter::Import() {
	UPoseAsset* PoseAsset = NewObject<UPoseAsset>(OutermostPkg, UPoseAsset::StaticClass(), *FileName, RF_Standalone | RF_Public);

	/* Set Skeleton, so we can use it in the uncooking process */
	GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(AssetData,
	{
		"Skeleton"
	}), PoseAsset);

	/* Reverse LocalSpacePose (cooked data) back to source data */
	ReverseCookLocalSpacePose(PoseAsset->GetSkeleton());

	/* Final operation to set properties */
	GetObjectSerializer()->DeserializeObjectProperties(AssetData, PoseAsset);

	/* If the user wants to specify a pose asset animation */
	if (UAnimSequence* OptionalAnimationSequence = GetSelectedAsset<UAnimSequence>(true)) {
		PoseAsset->SourceAnimation = OptionalAnimationSequence;
	}

	return OnAssetCreation(PoseAsset);
}

void IPoseAssetImporter::ReverseCookLocalSpacePose(USkeleton* Skeleton) const {
	/* If PoseContainer or Tracks don't exist, no need to perform any operations */
	if (
		!AssetData->HasField("PoseContainer") ||
		!AssetData->GetObjectField("PoseContainer")->HasField("Tracks")
	) {
		return;
	}
	
	const TSharedPtr<FJsonObject> PoseContainer = AssetData->GetObjectField(TEXT("PoseContainer"));
	const TArray<TSharedPtr<FJsonValue>> TracksJson = PoseContainer->GetArrayField("Tracks");
	const TArray<TSharedPtr<FJsonValue>> PosesJson = PoseContainer->GetArrayField(TEXT("Poses"));

	const int32 NumTracks = TracksJson.Num();

	for (TSharedPtr<FJsonValue> PoseValue : PosesJson) {
		TSharedPtr<FJsonObject> Pose = PoseValue->AsObject();
		
		if (!Pose.IsValid()) {
			continue;
		}
		
		/* Read the optimized LocalSpacePose array */
		TArray<TSharedPtr<FJsonValue>> LocalSpacePoseJson; {
			if (Pose->HasField(TEXT("LocalSpacePose"))) {
				LocalSpacePoseJson = Pose->GetArrayField(TEXT("LocalSpacePose"));
			}
		}

		/* Build a mapping from track index (as int32) to index into LocalSpacePose. */
		TMap<int32, int32> TrackToBufferIndex; {
			if (Pose->HasField(TEXT("TrackToBufferIndex"))) {
				const TArray<TSharedPtr<FJsonValue>> TrackToBufferJson = Pose->GetArrayField(TEXT("TrackToBufferIndex"));
			
				for (TSharedPtr<FJsonValue> TrackToBuffer : TrackToBufferJson) {
					const TSharedPtr<FJsonObject> TrackToBufferObject = TrackToBuffer->AsObject();
				
					if (TrackToBufferObject.IsValid()) {
						int32 Key = FCString::Atoi(*TrackToBufferObject->GetStringField(TEXT("Key")));
						int32 Value = FCString::Atoi(*TrackToBufferObject->GetStringField(TEXT("Value")));
					
						TrackToBufferIndex.Add(Key, Value);
					}
				}
			}
		}

		/* We take the cooked pose data [LocalSpacePoseJson] and convert it back to SourceLocalSpacePose */
		TArray<TSharedPtr<FJsonValue>> SourceLocalSpacePose;
		SourceLocalSpacePose.SetNum(NumTracks);

		for (int32 i = 0; i < NumTracks; i++) {
			/* DefaultTransform can either be default, or extracted from the base skeleton */
			FTransform DefaultTransform = FTransform::Identity; {
				if (Skeleton) {
					const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
					const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(FName(*TracksJson[i]->AsString()));
				
					if (BoneIndex != INDEX_NONE) {
						const TArray<FTransform> ReferencePose = Skeleton->GetRefLocalPoses();
					
						if (ReferencePose.IsValidIndex(BoneIndex)) {
							DefaultTransform = ReferencePose[BoneIndex];
						}
					}
				}
			}

			/* If a track is found, combine the transform data together (the engine cooks only the difference between the reference skeleton and the pose) */
			if (TrackToBufferIndex.Contains(i)) {
				const int32 LocalIndex = TrackToBufferIndex[i];
				
				if (LocalSpacePoseJson.IsValidIndex(LocalIndex)) {
					const TSharedPtr<FJsonObject> AdditiveJson = LocalSpacePoseJson[LocalIndex]->AsObject();
					if (!AdditiveJson.IsValid()) continue;
					
					const FTransform AdditiveTransform = GetTransformFromJson(AdditiveJson);

					FTransform FullTransform = DefaultTransform; {
						FullTransform.SetRotation(AdditiveTransform.GetRotation() * DefaultTransform.GetRotation());
						FullTransform.SetTranslation(DefaultTransform.GetTranslation() + AdditiveTransform.GetTranslation());
						FullTransform.SetScale3D(DefaultTransform.GetScale3D() + AdditiveTransform.GetScale3D());

						FullTransform.NormalizeRotation();
					}

					SourceLocalSpacePose[i] = MakeShareable(new FJsonValueObject(GetTransformJson(FullTransform)));
					continue;
				}
			}

			SourceLocalSpacePose[i] = MakeShareable(new FJsonValueObject(GetTransformJson(DefaultTransform)));
		}

		/* Update the Pose with SourceLocalSpacePose */
		Pose->SetArrayField(TEXT("SourceLocalSpacePose"), SourceLocalSpacePose);
	}
}
