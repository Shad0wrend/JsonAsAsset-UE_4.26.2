/* Copyright JAA Contributors 2024-2025 */

#pragma once

#include "Importers/Constructor/Importer.h"
#include "Engine/UserDefinedStruct.h"

class IUserDefinedStructImporter : public IImporter {
public:
	IUserDefinedStructImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg, const TArray<TSharedPtr<FJsonValue>>& AllJsonObjects, UClass* AssetClass):
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg, AllJsonObjects, AssetClass) {
	}

	virtual bool Import() override;

protected:
	FEdGraphPinType ResolvePropertyPinType(const TSharedPtr<FJsonObject>& PropertyJsonObject);
	
	UObject* LoadObjectFromJsonReference(const TSharedPtr<FJsonObject>& ParentJsonObject, const FString& ReferenceKey);
	
	void ImportPropertyIntoStruct(UUserDefinedStruct* Struct, const TSharedPtr<FJsonObject>& PropertyJsonObject);
};

REGISTER_IMPORTER(IUserDefinedStructImporter, {
	TEXT("UserDefinedStruct")
}, "User Defined Assets");
