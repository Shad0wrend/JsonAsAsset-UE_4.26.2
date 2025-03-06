// Copyright JAA Contributors 2024-2025

#include "Importers/Types/Curves/CurveFloatImporter.h"
#include "Factories/CurveFactory.h"
#include "Dom/JsonObject.h"

bool ICurveFloatImporter::Import() {
	// Quick way to access the curve keys
	TArray<TSharedPtr<FJsonValue>> Keys = JsonObject->GetObjectField(TEXT("Properties"))->GetObjectField(TEXT("FloatCurve"))->GetArrayField(TEXT("Keys"));

	UCurveFloatFactory* CurveFactory = NewObject<UCurveFloatFactory>();
	UCurveFloat* CurveAsset = Cast<UCurveFloat>(CurveFactory->FactoryCreateNew(UCurveFloat::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

	// Add Rich Keys
	for (TSharedPtr<FJsonValue>& Key : Keys) {
		CurveAsset->FloatCurve.Keys.Add(ObjectToRichCurveKey(Key->AsObject()));
	}

	// Handle edit changes, and add it to the content browser
	return OnAssetCreation(CurveAsset);
}