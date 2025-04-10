/* Copyright JsonAsAsset Contributors 2024-2025 */

#pragma once

#include "ObjectUtilities.h"
#include "Dom/JsonObject.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "PropertyUtilities.generated.h"

class UObjectSerializer;

USTRUCT()
struct FFailedPropertyInfo
{
	GENERATED_BODY()

public:
	FString ClassName;
	FString ObjectPath;
	FString SuperStructName;

	bool operator==(const FFailedPropertyInfo& Other) const
	{
		return ClassName == Other.ClassName &&
			   SuperStructName == Other.SuperStructName &&
			   ObjectPath == Other.ObjectPath;
	}
};

/** Handles struct serialization */
class JSONASASSET_API FStructSerializer
{
public:
	virtual ~FStructSerializer() = default;
	virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, TArray<int32>* OutReferencedSubobjects) = 0;
	virtual void Deserialize(UScriptStruct* Struct, void* StructData, const TSharedPtr<FJsonObject> JsonValue) = 0;
	virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) = 0;
};

/** Fallback struct serializer using default UE reflection */
class FFallbackStructSerializer : public FStructSerializer
{
	UPropertySerializer* PropertySerializer;

public:
	FFallbackStructSerializer(UPropertySerializer* PropertySerializer);

	virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructValue, TArray<int32>* OutReferencedSubobjects) override;
	virtual void Deserialize(UScriptStruct* Struct, void* StructValue, const TSharedPtr<FJsonObject> JsonValue) override;
	virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) override;
};

class FDateTimeSerializer : public FStructSerializer
{
public:
	virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, TArray<int32>* OutReferencedSubobjects) override;
	virtual void Deserialize(UScriptStruct* Struct, void* StructData, const TSharedPtr<FJsonObject> JsonValue) override;
	virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) override;
};

class FTimespanSerializer : public FStructSerializer
{
public:
	virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, TArray<int32>* OutReferencedSubobjects) override;
	virtual void Deserialize(UScriptStruct* Struct, void* StructData, const TSharedPtr<FJsonObject> JsonValue) override;
	virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) override;
};

/* A structure to hold data for a UObject export. */
struct FUObjectExport {
	FUObjectExport(): Object(nullptr), Parent(nullptr), Position(-1) {
	};

	FName Name;
	FName Type;
	FName Outer;

	/* The json object of the expression, this is not Properties */
	TSharedPtr<FJsonObject> JsonObject;

	/* Object created */
	UObject* Object;

	/* Parent of this expression */
	UObject* Parent;

	int Position;

	FUObjectExport(const FName& Name, const FName& Type, const FName Outer, const TSharedPtr<FJsonObject>& JsonObject, UObject* Object, UObject* Parent, int Position)
		: Name(Name), Type(Type), Outer(Outer), JsonObject(JsonObject), Object(Object), Parent(Parent), Position(Position) {
	}

	FUObjectExport(const FName& Name, const FName& Type, const FName Outer, const TSharedPtr<FJsonObject>& JsonObject, UObject* Object, UObject* Parent)
		: Name(Name), Type(Type), Outer(Outer), JsonObject(JsonObject), Object(Object), Parent(Parent), Position(-1) {
	}

	TSharedPtr<FJsonObject> GetProperties() const {
		TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));

		return Properties;
	}

	bool IsValid() const {
		return JsonObject != nullptr && Object != nullptr;
	}
};

struct FUObjectExportContainer {
	/* Array of Expression Exports */
	TArray<FUObjectExport> Exports;
	
	FUObjectExportContainer() {};

	FUObjectExport Find(const FName Name) {
		for (FUObjectExport Export : Exports) {
			if (Export.Name == Name) {
				return Export;
			}
		}

		return FUObjectExport();
	}

	FUObjectExport Find(const FName Name, const FName Outer) {
		for (FUObjectExport Export : Exports) {
			if (Export.Name == Name && Export.Outer == Outer) {
				return Export;
			}
		}

		return FUObjectExport();
	}

	FUObjectExport Find(const int Position) {
		for (FUObjectExport Export : Exports) {
			if (Export.Position == Position) {
				return Export;
			}
		}

		return FUObjectExport();
	}

	UObject* FindRef(const int Position) {
		for (const FUObjectExport Export : Exports) {
			if (Export.Position == Position) {
				return Export.Object;
			}
		}

		return nullptr;
	}

	FUObjectExport Find(const FString Name) {
		return Find(FName(*Name));
	}

	FUObjectExport Find(const FString Name, const FString Outer) {
		return Find(FName(*Name), FName(*Outer));
	}
	
	bool Contains(const FName Name) {
		for (FUObjectExport Export : Exports) {
			if (Export.Name == Name) {
				return true;
			}
		}

		return false;
	}

	void Empty() {
		Exports.Empty();
	}
	
	int Num() const {
		return Exports.Num();
	}
};

UCLASS()
class JSONASASSET_API UPropertySerializer : public UObject
{
	GENERATED_BODY()

	friend class UObjectSerializer;

	UPROPERTY()
	UObjectSerializer* ObjectSerializer;

	UPROPERTY()
	TArray<UStruct*> PinnedStructs;
	TArray<FProperty*> BlacklistedProperties;

	TSharedPtr<FStructSerializer> FallbackStructSerializer;
	TMap<UScriptStruct*, TSharedPtr<FStructSerializer>> StructSerializers;
	
public:
	UPropertySerializer();

	FUObjectExportContainer ExportsContainer;

	TArray<FString> BlacklistedPropertyNames;
	
	TArray<FFailedPropertyInfo> FailedProperties;
	void ClearCachedData();

	/** Disables property serialization entirely */
	void DisablePropertySerialization(UStruct* Struct, FName PropertyName);
	void AddStructSerializer(UScriptStruct* Struct, const TSharedPtr<FStructSerializer>& Serializer);

	/** Checks whenever we should serialize property in question at all */
	bool ShouldSerializeProperty(FProperty* Property) const;

	TSharedRef<FJsonValue> SerializePropertyValue(FProperty* Property, const void* Value, TArray<int32>* OutReferencedSubobjects = nullptr);
	TSharedRef<FJsonObject> SerializeStruct(UScriptStruct* Struct, const void* Value, TArray<int32>* OutReferencedSubobjects = nullptr);

	void DeserializePropertyValue(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
	void DeserializeStruct(UScriptStruct* Struct, const TSharedRef<FJsonObject>& Value, void* OutValue);

	bool ComparePropertyValues(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context = MakeShareable(new FObjectCompareContext));
	bool CompareStructs(UScriptStruct* Struct, const TSharedRef<FJsonObject>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context = MakeShareable(new FObjectCompareContext));

private:
	FStructSerializer* GetStructSerializer(UScriptStruct* Struct) const;
	bool ComparePropertyValuesInner(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context);
	TSharedRef<FJsonValue> SerializePropertyValueInner(FProperty* Property, const void* Value, TArray<int32>* OutReferencedSubobjects);
};

/* Use to handle differentiating formats produced by CUE4Parse */
inline bool PassthroughPropertyHandler(FProperty* Property, const FString& PropertyName, void* PropertyValue, const TSharedPtr<FJsonObject>& Properties, UPropertySerializer* PropertySerializer) {
	/* Handles static arrays in the format of: PropertyName[Index] */
	if (Property->ArrayDim != 1) {
		TArray<TSharedPtr<FJsonValue>> ArrayElements;

		/* Finds array elements with the format: PropertyName[Index] and sets them properly into an array */
		for (const auto& Pair : Properties->Values) {
			const FString& Key = Pair.Key;
			TSharedPtr<FJsonValue> Value = Pair.Value;

			/* If it doesn't start with the same property name */
			if (!Key.StartsWith(PropertyName)) continue;

			/* By default, it should be 0 */
			int32 CurrentArrayIndex = 0;

			/* If it is formatted like PropertyName[Index] */
			if (Key.Contains("[") && Key.Contains("]")) {
				int32 OpenBracketPos, CloseBracketPos;

				/* Find the index in PropertyName[Index] (integer) */
				if (Key.FindChar('[', OpenBracketPos) && Key.FindChar(']', CloseBracketPos) && CloseBracketPos > OpenBracketPos) {
					FString IndexStr = Key.Mid(OpenBracketPos + 1, CloseBracketPos - OpenBracketPos - 1);
					
					CurrentArrayIndex = FCString::Atoi(*IndexStr);
				}
			}

			/* Fail-save? */
			if (CurrentArrayIndex >= ArrayElements.Num()) {
				ArrayElements.SetNum(CurrentArrayIndex + 1);
			}
			
			ArrayElements[CurrentArrayIndex] = Value;
		}

		/* Array elements is filled up, now we set them in the property value */
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayElements.Num(); ArrayIndex++) {
			uint8* ArrayPropertyValue = static_cast<uint8*>(PropertyValue) + Property->ElementSize * ArrayIndex;

			if (!ArrayElements.IsValidIndex(ArrayIndex)) continue;

			TSharedPtr<FJsonValue> ArrayJsonElement = ArrayElements[ArrayIndex];

			/* Check to see if it's null */
			if (ArrayJsonElement == nullptr || ArrayJsonElement->IsNull()) continue;
			
			const TSharedRef<FJsonValue> ArrayJsonValue = ArrayJsonElement.ToSharedRef();

			PropertySerializer->DeserializePropertyValue(Property, ArrayJsonValue, ArrayPropertyValue);
		}

		return true;
	}

	return false;
}