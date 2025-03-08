/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Curves/CurveVectorImporter.h"
#include "Factories/CurveFactory.h"
#include "Curves/CurveVector.h"

bool ICurveVectorImporter::Import() {
	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

	/* Array of containers */
	TArray<TSharedPtr<FJsonValue>> FloatCurves = Properties->GetArrayField(TEXT("FloatCurves"));

	UCurveVectorFactory* CurveVectorFactory = NewObject<UCurveVectorFactory>();
	UCurveVector* CurveVectorAsset = Cast<UCurveVector>(CurveVectorFactory->FactoryCreateNew(UCurveVector::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

	/* For each container, get keys */
	for (int i = 0; i < FloatCurves.Num(); i++) {
		TArray<TSharedPtr<FJsonValue>> Keys = FloatCurves[i]->AsObject()->GetArrayField(TEXT("Keys"));
		CurveVectorAsset->FloatCurves[i].Keys.Empty();

		/* Add keys to the array */
		for (int j = 0; j < Keys.Num(); j++) {
			CurveVectorAsset->FloatCurves[i].Keys.Add(ObjectToRichCurveKey(Keys[j]->AsObject()));
		}
	}

	/* Handle edit changes, and add it to the content browser */
	return OnAssetCreation(CurveVectorAsset);
}