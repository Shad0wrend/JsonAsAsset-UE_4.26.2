/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Modules/Tools/ClearImportData.h"
#include "EditorFramework/AssetImportData.h"
#include "Utilities/EngineUtilities.h"

void FToolClearImportData::Execute() {
	TArray<FAssetData> AssetDataList = GetAssetsInSelectedFolder();

	if (AssetDataList.Num() == 0) {
		return;
	}

	for (const FAssetData& AssetData : AssetDataList) {
		if (!AssetData.IsValid()) continue;
		if (AssetData.AssetClass != "AnimSequence" && AssetData.AssetClass != "SkeletalMesh" && AssetData.AssetClass != "StaticMesh") continue;
		
		UObject* Asset = AssetData.GetAsset();
		if (Asset == nullptr) continue;

		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Asset)) {
			AnimSequence->AssetImportData->SourceData.SourceFiles.Empty();
			AnimSequence->Modify();
		}

		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset)) {
			StaticMesh->AssetImportData->SourceData.SourceFiles.Empty();
		}

		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset)) {
			SkeletalMesh->SetAssetImportData(nullptr);
			SkeletalMesh->Modify();
		}
	}
}
