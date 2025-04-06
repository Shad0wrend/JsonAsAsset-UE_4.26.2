/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Importers/Types/Animation/BlendSpaceImporter.h"

#if ENGINE_UE5
#include "Animation/BlendSpaceBase.h"
#else
#include "Animation/BlendSpace.h"
#endif

bool IBlendSpaceImporter::Import() {
#if ENGINE_UE5
	auto BlendSpace = NewObject<UBlendSpace>(Package, AssetClass, *FileName, RF_Public | RF_Standalone);
#else
	UBlendSpaceBase* BlendSpace = NewObject<UBlendSpaceBase>(Package, AssetClass, *FileName, RF_Public | RF_Standalone);
#endif
	
	BlendSpace->Modify();
	
	GetObjectSerializer()->DeserializeObjectProperties(AssetData, BlendSpace);

	/* Ensure internal state is refreshed after adding all samples */
	BlendSpace->ValidateSampleData();
	BlendSpace->MarkPackageDirty();
	BlendSpace->PostEditChange();
	BlendSpace->PostLoad();

	return OnAssetCreation(BlendSpace);
}