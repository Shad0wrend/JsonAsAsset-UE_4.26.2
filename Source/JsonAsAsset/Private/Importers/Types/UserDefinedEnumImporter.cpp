/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/UserDefinedEnumImporter.h"

#include "Engine/UserDefinedEnum.h"
#include "Kismet2/EnumEditorUtils.h"

bool IUserDefinedEnumImporter::Import() {
	/* Create the UserDefinedEnum */
	UUserDefinedEnum* UserDefinedEnum = NewObject<UUserDefinedEnum>(Package, *FileName, RF_Public | RF_Standalone);
	
	UserDefinedEnum->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
	UserDefinedEnum->Modify();

	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

	/* CppForm ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	UEnum::ECppForm CppForm = UEnum::ECppForm::Regular;

	if (JsonObject->HasField(TEXT("CppForm"))) {
		FString CppForm_String = JsonObject->GetStringField(TEXT("CppForm"));

		/*
		 * Selector based on text
		 * Seems like we can't use the normal EnumAsString because of some access error
		 */
		CppForm = CppForm_String == "Regular" ? UEnum::ECppForm::Regular : CppForm_String == "Namespaced" ? UEnum::ECppForm::Namespaced : UEnum::ECppForm::EnumClass;
	}

	/* EnumNames ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	if (JsonObject->HasTypedField<EJson::Object>(TEXT("Names"))) {
		TSharedPtr<FJsonObject> Names = JsonObject->GetObjectField(TEXT("Names"));

		/* Final EnumNames variable */
		TArray<TPair<FName, int64>> EnumNames;

		int32 EntryCount = Names->Values.Num();
		
		for (const auto& Pair : Names->Values)
		{
			/* Last entry is the _MAX name, automatically created by the engine */
			if (--EntryCount == 0) break;

			EnumNames.Emplace(FName(*Pair.Key), static_cast<int64>(Pair.Value->AsNumber()));
		}

		/* Update the enumeration with the enum names */
		UserDefinedEnum->SetEnums(EnumNames, CppForm, EEnumFlags::None, true);
	}
	
	/* DisplayNameMap ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	TArray<TSharedPtr<FJsonValue>> DisplayNameMap = Properties->GetArrayField(TEXT("DisplayNameMap"));
	TMap<FName, FText> DisplayNames;

	for (const TSharedPtr<FJsonValue>& DisplayEntry : DisplayNameMap) {
		if (!DisplayEntry.IsValid()) continue;

		TSharedPtr<FJsonObject> EntryObject = DisplayEntry->AsObject(); {
			if (!EntryObject.IsValid()) continue;
		}

		TSharedPtr<FJsonObject> ValueObject = EntryObject->GetObjectField(TEXT("Value")); {
			if (!ValueObject.IsValid()) continue;
		}

		/* Retrieve properties */
		FName EnumKey = *EntryObject->GetStringField(TEXT("Key"));
		FString TextNamespace = ValueObject->GetStringField(TEXT("Namespace"));
		FString UniqueKey = ValueObject->GetStringField(TEXT("Key"));
		FString SourceString = ValueObject->GetStringField(TEXT("SourceString"));

		/* TODO: Add LocalizedString */
		DisplayNames.Add(EnumKey, FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *TextNamespace, *UniqueKey));
	}

	/* Set Display Names in the UserDefinedEnum */
	for (const auto& Pair : DisplayNames) {
		UserDefinedEnum->DisplayNameMap.Add(Pair.Key, Pair.Value);
	}

	/* Finalization ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	UserDefinedEnum->MarkPackageDirty();
	UserDefinedEnum->Modify();
	UserDefinedEnum->PostEditChange();

	FEnumEditorUtils::EnsureAllDisplayNamesExist(UserDefinedEnum);

	/* Handle edit changes, and add it to the content browser */
	return OnAssetCreation(UserDefinedEnum);
}
