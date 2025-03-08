// Copyright JAA Contributors 2024-2025

#include "Importers/Types/Tables/StringTableImporter.h"

#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"

bool IStringTableImporter::Import() {
	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

	/* Create StringTable from Package */
	UStringTable* StringTable = NewObject<UStringTable>(Package, UStringTable::StaticClass(), *FileName, RF_Public | RF_Standalone);

	if (Properties->HasField(TEXT("StringTable"))) {
		TSharedPtr<FJsonObject> AssetData = Properties->GetObjectField(TEXT("StringTable"));
		FStringTableRef MutableStringTable = StringTable->GetMutableStringTable();

		/* Set Table Namespace */
		MutableStringTable->SetNamespace(AssetData->GetStringField(TEXT("TableNamespace")));

		/* Set "SourceStrings" from KeysToEntries */
		const TSharedPtr<FJsonObject> KeysToEntries = AssetData->GetObjectField(TEXT("KeysToEntries"));
	
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : KeysToEntries->Values) {
			FString Key = Pair.Key;
			FString SourceString = Pair.Value->AsString();

			MutableStringTable->SetSourceString(Key, SourceString);
		}

		/* Set Metadata from KeysToMetaData */
		const TSharedPtr<FJsonObject> KeysToMetaData = AssetData->GetObjectField(TEXT("KeysToMetaData"));

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : KeysToMetaData->Values) {
			const FString TableKey = Pair.Key;
			const TSharedPtr<FJsonObject> MetadataObject = Pair.Value->AsObject();

			for (const TPair<FString, TSharedPtr<FJsonValue>>& MetadataPair : MetadataObject->Values) {
				FName TextKey = *MetadataPair.Key;
				FString MetadataValue = MetadataPair.Value->AsString();
			
				MutableStringTable->SetMetaData(TableKey, TextKey, MetadataValue);
			}
		}
	}

	// Handle edit changes, and add it to the content browser
	return OnAssetCreation(StringTable);
}
