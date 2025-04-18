/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Utilities/Serializers/ObjectUtilities.h"
#include "Utilities/AppStyleCompatibility.h"

#if ENGINE_UE5
#include "AnimGraphNode_Base.h"
#else
#include "AnimGraph/Classes/AnimGraphNode_Base.h"
#endif

#include "Utilities/Serializers/PropertyUtilities.h"
#include "UObject/Package.h"
#include "Utilities/EngineUtilities.h"

// ReSharper disable once CppDeclaratorNeverUsed
DECLARE_LOG_CATEGORY_CLASS(LogObjectSerializer, All, All);
PRAGMA_DISABLE_OPTIMIZATION

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

UObjectSerializer::UObjectSerializer(): ParentAsset(nullptr), PropertySerializer(nullptr) {
}

void UObjectSerializer::SetupExports(const TArray<TSharedPtr<FJsonValue>>& InObjects) {
	Exports = InObjects;
	
	PropertySerializer->ClearCachedData();
}

UPackage* FindOrLoadPackage(const FString& PackageName) {
	UPackage* Package = FindPackage(nullptr, *PackageName);
	
	if (!Package) {
		Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	}

	return Package;
}

void UObjectSerializer::SetPropertySerializer(UPropertySerializer* NewPropertySerializer) {
	check(NewPropertySerializer);

	this->PropertySerializer = NewPropertySerializer;
	NewPropertySerializer->ObjectSerializer = this;
}

void UObjectSerializer::SetExportForDeserialization(const TSharedPtr<FJsonObject>& Object) {
	ExportsToNotDeserialize.Add(Object->GetStringField(TEXT("Name")));
}

void UObjectSerializer::DeserializeExports(TArray<TSharedPtr<FJsonValue>> InExports) {
	PropertySerializer->ExportsContainer.Empty();
	
	TMap<TSharedPtr<FJsonObject>, UObject*> ExportsMap;
	int Index = -1;
	
	for (TSharedPtr<FJsonValue> Object : InExports) {
		Index++;
		
		TSharedPtr<FJsonObject> ExportObject = Object->AsObject();

		/* No name = no export!! */
		if (!ExportObject->HasField(TEXT("Name"))) continue;

		FString Name = ExportObject->GetStringField(TEXT("Name"));
		FString Type = ExportObject->GetStringField(TEXT("Type"));
		
		/* Check if it's not supposed to be deserialized */
		if (ExportsToNotDeserialize.Contains(Name)) continue;
		if (Type == "BodySetup" || Type == "NavCollision") continue;

		FString ClassName = ExportObject->GetStringField(TEXT("Class"));

		if (ExportObject->HasField("Template")) {
			const TSharedPtr<FJsonObject> TemplateObject = ExportObject->GetObjectField(TEXT("Template"));
			ClassName = ReadPathFromObject(&TemplateObject).Replace(TEXT("Default__"), TEXT(""));
		}

		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);

		if (!Class) {
			Class = FindObject<UClass>(ANY_PACKAGE, *Type);
		}

		if (!Class) continue;

		FString Outer = ExportObject->GetStringField(TEXT("Outer"));
		UObject* ObjectOuter = ParentAsset;

		if (FUObjectExport Export = PropertySerializer->ExportsContainer.Find(Outer); Export.Object != nullptr) {
			UObject* FoundObject = Export.Object;
			ObjectOuter = FoundObject;
		}

		UObject* NewUObject = NewObject<UObject>(ObjectOuter, Class, FName(*Name));

		if (ExportObject->HasField(TEXT("Properties"))) {
			TSharedPtr<FJsonObject> Properties = ExportObject->GetObjectField(TEXT("Properties"));

			ExportsMap.Add(Properties, NewUObject);
		}

		/* Add it to the referenced objects */
		PropertySerializer->ExportsContainer.Exports.Add(FUObjectExport(FName(*Name), FName(*Type), FName(*Outer), ExportObject, NewUObject, ParentAsset, Index));

		/* Already deserialized */
		ExportsToNotDeserialize.Add(Name);
	}

	for (const auto Pair : ExportsMap) {
		TSharedPtr<FJsonObject> Properties = Pair.Key;
		UObject* Object = Pair.Value;

		DeserializeObjectProperties(Properties, Object);
	}
}

void UObjectSerializer::DeserializeObjectProperties(const TSharedPtr<FJsonObject>& Properties, UObject* Object) const {
	if (Object == nullptr) return;

	const UClass* ObjectClass = Object->GetClass();

	for (FProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext) {
		const FString PropertyName = Property->GetName();

		if (!PropertySerializer->ShouldDeserializeProperty(Property)) continue;

		void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Object);
		const bool HasHandledProperty = PassthroughPropertyHandler(Property, PropertyName, PropertyValue, Properties, PropertySerializer);

		/* Handler Specifically for Animation Blueprint Graph Nodes */
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property)) {
			if (StructProperty->Struct->IsChildOf(FAnimNode_Base::StaticStruct())) {
				void* StructPtr = StructProperty->ContainerPtrToValuePtr<void>(Object);
				const FAnimNode_Base* AnimNode = static_cast<FAnimNode_Base*>(StructPtr);

				if (AnimNode) {
					PropertySerializer->DeserializeStruct(StructProperty->Struct, Properties.ToSharedRef(), PropertyValue);
				}
			}
		}
		
		if (Properties->HasField(PropertyName) && !HasHandledProperty && PropertyName != "LODParentPrimitive") {
			const TSharedPtr<FJsonValue>& ValueObject = Properties->Values.FindChecked(PropertyName);

			if (Property->ArrayDim == 1 || ValueObject->Type == EJson::Array) {
				PropertySerializer->DeserializePropertyValue(Property, ValueObject.ToSharedRef(), PropertyValue);
			}
		}
	}

	/* this is a use case for importing maps and parsing static mesh components
	 * using the object and property serializer, this was initially wanted to be
	 * done completely without any manual work (using the de-serializers)
	 * however I don't think it's possible to do so. as I haven't seen any native
	 * property that can do this using the data provided in CUE4Parse
	 */
	if (Properties->HasField(TEXT("LODData")) && Cast<UStaticMeshComponent>(Object)) {
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Object);
		if (!StaticMeshComponent) return;
		
		TArray<TSharedPtr<FJsonValue>> ObjectLODData = Properties->GetArrayField(TEXT("LODData"));
		int CurrentLOD = -1;
		
		for (const TSharedPtr<FJsonValue> CurrentLODValue : ObjectLODData) {
			CurrentLOD++;

			const TSharedPtr<FJsonObject> CurrentLODObject = CurrentLODValue->AsObject();

			/* Must contain vertex colors, or else it's an empty LOD */
			if (!CurrentLODObject->HasField(TEXT("OverrideVertexColors"))) continue;

			const TSharedPtr<FJsonObject> OverrideVertexColorsObject = CurrentLODObject->GetObjectField(TEXT("OverrideVertexColors"));

			if (!OverrideVertexColorsObject->HasField(TEXT("Data"))) continue;

			const int32 NumVertices = OverrideVertexColorsObject->GetIntegerField(TEXT("NumVertices"));
			const TArray<TSharedPtr<FJsonValue>> DataArray = OverrideVertexColorsObject->GetArrayField(TEXT("Data"));

			/* Template of the target data */
			FString Output = FString::Printf(TEXT("CustomLODData LOD=%d, ColorVertexData(%d)=("), CurrentLOD, NumVertices);

			/* Append the colors in the expected format */
			for (int32 i = 0; i < DataArray.Num(); ++i) {
				FString ColorValue = DataArray.operator[](i)->AsString();
				Output.Append(ColorValue);

				/* Add a comma unless it's the last element */
				if (i < DataArray.Num() - 1) {
					Output.Append(TEXT(","));
				}
			}

			Output.Append(TEXT(")"));
		
			StaticMeshComponent->ImportCustomProperties(*Output, GWarn);
		}
	}
}

PRAGMA_ENABLE_OPTIMIZATION