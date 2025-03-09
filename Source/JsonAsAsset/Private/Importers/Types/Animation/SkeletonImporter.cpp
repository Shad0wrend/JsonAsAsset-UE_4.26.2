/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Animation/SkeletonImporter.h"

bool ISkeletonImporter::Import() {
	USkeleton* Skeleton = GetSelectedAsset<USkeleton>(true);

	/*
	 * If there is no skeleton, create one.
	 */
	if (!Skeleton) {
		Skeleton = NewObject<USkeleton>(Package, USkeleton::StaticClass(), *FileName, RF_Public | RF_Standalone);

		ApplySkeletalChanges(Skeleton);
	} else {
		/* Empty the skeleton's sockets, blend profiles, and virtual bones */
		Skeleton->Sockets.Empty();
		Skeleton->BlendProfiles.Empty();

		TArray<FName> VirtualBoneNames;

		for (const FVirtualBone VirtualBone : Skeleton->GetVirtualBones()) {
			VirtualBoneNames.Add(VirtualBone.VirtualBoneName);
		}
		
		Skeleton->RemoveVirtualBones(VirtualBoneNames);
	}

	UObjectSerializer* ObjectSerializer = GetObjectSerializer();
	ObjectSerializer->SetPackageForDeserialization(Package);
	ObjectSerializer->SetExportForDeserialization(JsonObject);
	ObjectSerializer->ParentAsset = Skeleton;

	ObjectSerializer->DeserializeExports(AllJsonObjects);

	GetObjectSerializer()->DeserializeObjectProperties(AssetData, Skeleton);

	return HandleAssetCreation(Skeleton);
}

void ISkeletonImporter::ApplySkeletalChanges(USkeleton* Skeleton) const {
	TSharedPtr<FJsonObject> ReferenceSkeletonObject = AssetData->GetObjectField(TEXT("ReferenceSkeleton"));

	/* Get access to ReferenceSkeleton */
	FReferenceSkeleton& ReferenceSkeleton = const_cast<FReferenceSkeleton&>(Skeleton->GetReferenceSkeleton());
	FReferenceSkeletonModifier ReferenceSkeletonModifier(ReferenceSkeleton, Skeleton);

	TArray<TSharedPtr<FJsonValue>> FinalRefBoneInfo = ReferenceSkeletonObject->GetArrayField(TEXT("FinalRefBoneInfo"));
	TArray<TSharedPtr<FJsonValue>> FinalRefBonePose = ReferenceSkeletonObject->GetArrayField(TEXT("FinalRefBonePose"));

	int BoneIndex = 0;

	/* Go through each bone reference */
	for (TSharedPtr<FJsonValue> FinalReferenceBoneInfoValue : FinalRefBoneInfo) {
		TSharedPtr<FJsonObject> FinalReferenceBoneInfo = FinalReferenceBoneInfoValue->AsObject();

		FName Name(*FinalReferenceBoneInfo->GetStringField(TEXT("Name")));
		int ParentIndex = FinalReferenceBoneInfo->GetIntegerField(TEXT("ParentIndex"));

		/* Fail-safe */
		if (!FinalRefBonePose.IsValidIndex(BoneIndex) || !FinalRefBonePose[BoneIndex].IsValid()) {
			continue;
		}
		
		TSharedPtr<FJsonObject> BonePoseTransform = FinalRefBonePose[BoneIndex]->AsObject();
		FTransform Transform; {
			PropertySerializer->DeserializeStruct(TBaseStructure<FTransform>::Get(), BonePoseTransform.ToSharedRef(), &Transform);
		}

		FMeshBoneInfo MeshBoneInfo = FMeshBoneInfo(Name, "", ParentIndex);

		/* Add the bone */
		ReferenceSkeletonModifier.Add(MeshBoneInfo, Transform);

		BoneIndex++;
	}

	/* Re-build skeleton */
	ReferenceSkeleton.RebuildRefSkeleton(Skeleton, true);
	
	Skeleton->ClearCacheData();
	Skeleton->MarkPackageDirty();
}