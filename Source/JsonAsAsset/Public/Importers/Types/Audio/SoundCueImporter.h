// Copyright JAA Contributors 2024-2025

#pragma once

#include "Importers/Constructor/Graph/SoundGraph.h"

class ISoundCueImporter : public ISoundGraph {
public:
	ISoundCueImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects):
		ISoundGraph(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects) {
	}

	virtual bool Import() override;
};
