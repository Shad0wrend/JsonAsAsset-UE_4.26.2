// Copyright JAA Contributors 2024-2025

#include "Importers/Types/Physics/PhysicsAssetImporter.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

#include "Utilities/EngineUtilities.h"

USkeletalBodySetup* IPhysicsAssetImporter::CreateNewBody(UPhysicsAsset* PhysAsset, FName ExportName, FName BoneName)
{
	USkeletalBodySetup* NewBodySetup = NewObject<USkeletalBodySetup>(PhysAsset, ExportName, RF_Transactional);
	NewBodySetup->BoneName = BoneName;

	PhysAsset->SkeletalBodySetups.Add(NewBodySetup);

	return NewBodySetup;
}

UPhysicsConstraintTemplate* IPhysicsAssetImporter::CreateNewConstraint(UPhysicsAsset* PhysAsset, FName ExportName)
{
	UPhysicsConstraintTemplate* NewConstraintSetup = NewObject<UPhysicsConstraintTemplate>(PhysAsset, ExportName, RF_Transactional);
	PhysAsset->ConstraintSetup.Add(NewConstraintSetup);

	return NewConstraintSetup;
}

bool IPhysicsAssetImporter::Import()
{
	UPhysicsAsset* PhysicsAsset = NewObject<UPhysicsAsset>(Package, UPhysicsAsset::StaticClass(), *FileName, RF_Public | RF_Standalone);

	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));
	TMap<FName, FExportData> Exports = CreateExports();
	
	/* SkeletalBodySetups ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	ProcessJsonArrayField(Properties, TEXT("SkeletalBodySetups"), [&](const TSharedPtr<FJsonObject>& ObjectField) {
		FName ExportName = GetExportNameOfSubobject(ObjectField->GetStringField(TEXT("ObjectName")));
		FJsonObject* ExportJson = Exports.Find(ExportName)->Json;

		TSharedPtr<FJsonObject> ExportProperties = ExportJson->GetObjectField(TEXT("Properties"));
		FName BoneName = FName(*ExportProperties->GetStringField(TEXT("BoneName")));
		
		USkeletalBodySetup* BodySetup = CreateNewBody(PhysicsAsset, ExportName, BoneName);

		GetObjectSerializer()->DeserializeObjectProperties(ExportProperties, BodySetup);
	});

	/* For caching. IMPORTANT! DO NOT REMOVE! */
	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->UpdateBoundsBodiesArray();

	/* CollisionDisableTable ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	TArray<TSharedPtr<FJsonValue>> CollisionDisableTable = JsonObject->GetArrayField(TEXT("CollisionDisableTable"));

	for (const TSharedPtr<FJsonValue> TableJSONElement : CollisionDisableTable)
	{
		const TSharedPtr<FJsonObject> TableObjectElement = TableJSONElement->AsObject();

		bool MapValue = TableObjectElement->GetBoolField(TEXT("Value"));
		TArray<TSharedPtr<FJsonValue>> Indices = TableObjectElement->GetObjectField(TEXT("Key"))->GetArrayField(TEXT("Indices"));

		int32 BodyIndexA = Indices[0]->AsNumber();
		int32 BodyIndexB = Indices[1]->AsNumber();

		/* Add to the CollisionDisableTable */
		PhysicsAsset->CollisionDisableTable.Add(FRigidBodyIndexPair(BodyIndexA, BodyIndexB), MapValue);
	}

	/* ConstraintSetup ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	ProcessJsonArrayField(Properties, TEXT("ConstraintSetup"), [&](const TSharedPtr<FJsonObject>& ObjectField) {
		FName ExportName = GetExportNameOfSubobject(ObjectField->GetStringField(TEXT("ObjectName")));
		FJsonObject* ExportJson = Exports.Find(ExportName)->Json;

		TSharedPtr<FJsonObject> ExportProperties = ExportJson->GetObjectField(TEXT("Properties"));
		UPhysicsConstraintTemplate* PhysicsConstraintTemplate = CreateNewConstraint(PhysicsAsset, ExportName);
		
		GetObjectSerializer()->DeserializeObjectProperties(ExportProperties, PhysicsConstraintTemplate);

		/* For caching. IMPORTANT! DO NOT REMOVE! */
		PhysicsConstraintTemplate->UpdateProfileInstance();
	});

	/* Simple data at end ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(Properties,
	{
		"SkeletalBodySetups",
		"ConstraintSetup",
		"BoundsBodies",
		"ThumbnailInfo"
	}), PhysicsAsset);

	/* If the user selected a skeletal mesh in the browser, set it in the physics asset */
	const USkeletalMesh* SkeletalMesh = GetSelectedAsset<USkeletalMesh>(true);
	
	if (SkeletalMesh)
	{
		PhysicsAsset->PreviewSkeletalMesh = SkeletalMesh;
		PhysicsAsset->PostEditChange();
	}
	
	/* Finalize ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	PhysicsAsset->Modify();
	PhysicsAsset->MarkPackageDirty();
	PhysicsAsset->UpdateBoundsBodiesArray();
	
	return OnAssetCreation(PhysicsAsset);
}
