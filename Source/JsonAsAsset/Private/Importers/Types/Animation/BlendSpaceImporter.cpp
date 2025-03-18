/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Importers/Types/Animation/BlendSpaceImporter.h"
#include "Animation/BlendSpace.h"

bool IBlendSpaceImporter::Import() {
	UBlendSpace* BlendSpace = NewObject<UBlendSpace>(Package, AssetClass, *FileName, RF_Public | RF_Standalone);
	
	BlendSpace->Modify();
	
	GetObjectSerializer()->DeserializeObjectProperties(AssetData, BlendSpace);

	/* Ensure internal state is refreshed after adding all samples */
	BlendSpace->ValidateSampleData();
	BlendSpace->MarkPackageDirty();
	BlendSpace->PostEditChange();
	BlendSpace->PostLoad();

	return OnAssetCreation(BlendSpace);
}