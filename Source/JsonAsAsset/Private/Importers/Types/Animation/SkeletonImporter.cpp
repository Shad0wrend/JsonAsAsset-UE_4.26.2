// Copyright JAA Contributors 2024-2025

#include "Importers/Types/Animation/SkeletonImporter.h"
#include "Dom/JsonObject.h"

bool ISkeletonImporter::Import() {
	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));
	USkeleton* Skeleton = GetSelectedAsset<USkeleton>();

	/* Must have a skeleton selected */
	if (!Skeleton) return false;

	Skeleton->Sockets.Empty();

	SetupSerializer(Skeleton);
	
	GetObjectSerializer()->DeserializeExports(AllJsonObjects);
	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(Properties,
	{
		"FinalRefBonePose",
		"FinalNameToIndexMap",
		"Guid",
		"ReferenceSkeleton"
	}), Skeleton);

	Skeleton->Modify();
	Skeleton->PostEditChange();

	return true;
}
