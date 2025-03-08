/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Animation/SkeletonImporter.h"

bool ISkeletonImporter::Import() {
	USkeleton* Skeleton = GetSelectedAsset<USkeleton>();

	/* Must have a skeleton selected */
	if (!Skeleton) return false;

	Skeleton->Sockets.Empty();

	SetupSerializer(Skeleton);
	
	GetObjectSerializer()->DeserializeExports(AllJsonObjects);
	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(AssetData,
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
