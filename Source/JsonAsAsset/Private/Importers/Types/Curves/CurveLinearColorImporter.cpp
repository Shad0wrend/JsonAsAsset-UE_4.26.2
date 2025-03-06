// Copyright JAA Contributors 2024-2025

#include "Importers/Types/Curves/CurveLinearColorImporter.h"
#include "Curves/CurveLinearColor.h"
#include "Factories/CurveFactory.h"
#include "UObject/SavePackage.h"

bool ICurveLinearColorImporter::Import() {
	// Array of containers
	TArray<TSharedPtr<FJsonValue>> FloatCurves = JsonObject->GetArrayField(TEXT("FloatCurves"));

	UCurveLinearColorFactory* CurveFactory = NewObject<UCurveLinearColorFactory>();
	UCurveLinearColor* LinearCurveAsset = Cast<UCurveLinearColor>(CurveFactory->FactoryCreateNew(UCurveLinearColor::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

	// for each container, get keys
	for (int i = 0; i < FloatCurves.Num(); i++) {
		TArray<TSharedPtr<FJsonValue>> Keys = FloatCurves[i]->AsObject()->GetArrayField(TEXT("Keys"));
		LinearCurveAsset->FloatCurves[i].Keys.Empty();

		// add keys to array
		for (int j = 0; j < Keys.Num(); j++) {
			LinearCurveAsset->FloatCurves[i].Keys.Add(ObjectToRichCurveKey(Keys[j]->AsObject()));
		}
	}

	return OnAssetCreation(LinearCurveAsset);
}
