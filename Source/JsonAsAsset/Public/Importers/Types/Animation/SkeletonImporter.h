// Copyright JAA Contributors 2024-2025

#pragma once

#include "Importers/Constructor/Importer.h"

class ISkeletonImporter : public IImporter {
public:
	ISkeletonImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, TArray<TSharedPtr<FJsonValue>>& JsonObjects):
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, JsonObjects) {
	}

	virtual bool Import() override;
};
