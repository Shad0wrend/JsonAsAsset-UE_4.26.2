/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Graph/SoundGraph.h"

class ISoundCueImporter : public ISoundGraph {
public:
	ISoundCueImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		ISoundGraph(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;
};

REGISTER_IMPORTER(ISoundCueImporter, {
	"SoundCue"
}, "Sound Assets");