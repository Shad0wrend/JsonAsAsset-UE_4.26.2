/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Modules/Tools/ConvexCollision.h"
#include "Utilities/EngineUtilities.h"

void FToolConvexCollision::Execute() {
	TArray<FAssetData> AssetDataList = GetAssetsInSelectedFolder();

	if (AssetDataList.Num() == 0) {
		return;
	}
	
	/* Create an object serializer */
	UObjectSerializer* ObjectSerializer = CreateObjectSerializer();

	for (const FAssetData& AssetData : AssetDataList) {
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset())) {
			FString ObjectPath = AssetData.ObjectPath.ToString();

			const TSharedPtr<FJsonObject> Response = FAssetUtilities::API_RequestExports(ObjectPath);
			if (Response == nullptr || ObjectPath.IsEmpty()) continue;

			/* Not found */
			if (Response->HasField(TEXT("errored"))) {
				continue;
			}

			TArray<TSharedPtr<FJsonValue>> DataObjects = Response->GetArrayField(TEXT("jsonOutput"));
			
			/* Get Body Setup (different in Unreal Engine versions) */
#if !(ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 27)
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
#else
			UBodySetup* BodySetup = StaticMesh->BodySetup;
#endif

			for (const TSharedPtr<FJsonValue>& DataObject : DataObjects) {
				if (!DataObject.IsValid() || !DataObject->AsObject().IsValid()) {
					continue;
				}

				TSharedPtr<FJsonObject> JsonObject = DataObject->AsObject();
				FString TypeValue;

				/* Check if the "Type" field exists and matches "BodySetup" */
				if (JsonObject->TryGetStringField(TEXT("Type"), TypeValue) && TypeValue == "BodySetup") {
					/* Check for "Class" with value "UScriptClass'BodySetup'" */
					FString ClassValue;
					if (JsonObject->TryGetStringField(TEXT("Class"), ClassValue) && ClassValue == "UScriptClass'BodySetup'") {
						/* Navigate to "Properties" */
						TSharedPtr<FJsonObject> PropertiesObject = JsonObject->GetObjectField(TEXT("Properties"));
						if (PropertiesObject.IsValid()) {
							/* Navigate to "AggGeom" */
							TSharedPtr<FJsonObject> AggGeomObject = PropertiesObject->GetObjectField(TEXT("AggGeom"));
							if (AggGeomObject.IsValid()) {
								FKAggregateGeom AggGeom;

								ObjectSerializer->DeserializeObjectProperties(PropertiesObject, BodySetup);
								BodySetup->CollisionTraceFlag = CTF_UseDefault;
								StaticMesh->MarkPackageDirty();
								BodySetup->PostEditChange();
								StaticMesh->Modify(true);

								/* Notify the editor about the changes */
								BodySetup->InvalidatePhysicsData();
								BodySetup->CreatePhysicsMeshes();

								/* Notification */
								AppendNotification(
									FText::FromString("Imported Convex Collision: " + StaticMesh->GetName()),
									FText::FromString(StaticMesh->GetName()),
									3.5f,
									FAppStyle::GetBrush("PhysicsAssetEditor.EnableCollision.Small"),
									SNotificationItem::CS_Success,
									false,
									310.0f
								);
							}
						}
					}
				}
			}
		}
	}
}
