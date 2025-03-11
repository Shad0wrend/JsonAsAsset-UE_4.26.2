/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Importer.h"
#include "Engine/CurveTable.h"

class CCurveTableDerived : public UCurveTable {
public:
	void ChangeTableMode(ECurveTableMode Mode);
};

class ICurveTableImporter : public IImporter {
public:
	ICurveTableImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;
};

REGISTER_IMPORTER(ICurveTableImporter, {
	"CurveTable"
}, "Table Assets");