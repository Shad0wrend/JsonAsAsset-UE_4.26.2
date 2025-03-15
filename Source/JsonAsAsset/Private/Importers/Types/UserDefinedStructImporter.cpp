/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/UserDefinedStructImporter.h"

#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"

static const FRegexPattern PropertyNameRegexPattern(TEXT(R"((.*)_(\d+)_([0-9A-Z]+))"));

static const TMap<FString, const FName> PropertyCategoryMap = {
    {TEXT("BoolProperty"), TEXT("bool")},
    {TEXT("ByteProperty"), TEXT("byte")},
    {TEXT("IntProperty"), TEXT("int")},
    {TEXT("Int64Property"), TEXT("int64")},
    {TEXT("FloatProperty"), TEXT("real")},
    {TEXT("DoubleProperty"), TEXT("real")},
    {TEXT("StrProperty"), TEXT("string")},
    {TEXT("TextProperty"), TEXT("text")},
    {TEXT("NameProperty"), TEXT("name")},
    {TEXT("ClassProperty"), TEXT("class")},
    {TEXT("SoftClassProperty"), TEXT("softclass")},
    {TEXT("ObjectProperty"), TEXT("object")},
    {TEXT("SoftObjectProperty"), TEXT("softobject")},
    {TEXT("EnumProperty"), TEXT("byte")},
    {TEXT("StructProperty"), TEXT("struct")},
};

static const TMap<FString, EPinContainerType> ContainerTypeMap = {
    {TEXT("ArrayProperty"), EPinContainerType::Array},
    {TEXT("MapProperty"), EPinContainerType::Map},
    {TEXT("SetProperty"), EPinContainerType::Set},
};

bool IUserDefinedStructImporter::Import() {
    UUserDefinedStruct* UserDefinedStruct = FStructureEditorUtils::CreateUserDefinedStruct(Package, *FileName, RF_Standalone | RF_Public | RF_Transactional);

    GetObjectSerializer()->DeserializeObjectProperties(KeepPropertiesShared(AssetData,
    {
        "Guid",
        "DefaultProperties",
        "StructFlags"
    }), UserDefinedStruct);

    /* Remove default variable */
    FStructureEditorUtils::GetVarDesc(UserDefinedStruct).Pop();

    const TArray<TSharedPtr<FJsonValue>> ChildProperties = AssetData->GetArrayField(TEXT("ChildProperties"));
    
    for (const TSharedPtr<FJsonValue> Property : ChildProperties) {
        const TSharedPtr<FJsonObject> PropertyObject = Property->AsObject();
        
        ImportPropertyIntoStruct(UserDefinedStruct, PropertyObject);
    }

    /* Handle edit changes, and add it to the content browser */
    return OnAssetCreation(UserDefinedStruct);
}

void IUserDefinedStructImporter::ImportPropertyIntoStruct(UUserDefinedStruct *Struct, const TSharedPtr<FJsonObject> &PropertyJsonObject) {
    const FString Name = PropertyJsonObject->GetStringField(TEXT("Name"));
    const FString Type = PropertyJsonObject->GetStringField(TEXT("Type"));

    FString FieldDisplayName = Name;
    FString FieldId = "1";
    FGuid FieldGuid;

    FRegexMatcher RegexMatcher(PropertyNameRegexPattern, Name);
    
    if (RegexMatcher.FindNext()) {
        /* Import properties keeping GUID if present */
        FieldDisplayName = RegexMatcher.GetCaptureGroup(1);
        FieldId = RegexMatcher.GetCaptureGroup(2);
        FieldGuid = FGuid(RegexMatcher.GetCaptureGroup(3));
    } else {
        const uint32 UniqueNameId = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData)->GenerateUniqueNameIdForMemberVariable();

        FieldId = FString::FromInt(UniqueNameId);
        FieldGuid = FGuid::NewGuid();
    }

    const FName FormattedName = *FString::Printf(TEXT("%s_%s_%s"), *FieldDisplayName, *FieldId, *FieldGuid.ToString(EGuidFormats::Digits));

    FStructVariableDescription Variable; {
        Variable.VarName = FormattedName;
        Variable.FriendlyName = FieldDisplayName;
        Variable.VarGuid = FieldGuid;
        
        Variable.SetPinType(ResolvePropertyPinType(PropertyJsonObject)); 
    }

    FStructureEditorUtils::GetVarDesc(Struct).Add(Variable);
    FStructureEditorUtils::OnStructureChanged(Struct, FStructureEditorUtils::EStructureEditorChangeInfo::AddedVariable);
}

FEdGraphPinType IUserDefinedStructImporter::ResolvePropertyPinType(const TSharedPtr<FJsonObject> &PropertyJsonObject) {
    const FString Type = PropertyJsonObject->GetStringField(TEXT("Type"));

    /* Special handling for containers */
    const EPinContainerType *ContainerType = ContainerTypeMap.Find(Type);
    
    if (ContainerType) {
        if (*ContainerType == EPinContainerType::Map) {
            TSharedPtr<FJsonObject> KeyPropObject = PropertyJsonObject->GetObjectField(TEXT("KeyProp"));
            
            FEdGraphPinType ResolvedType = ResolvePropertyPinType(KeyPropObject);
            ResolvedType.ContainerType = *ContainerType;

            TSharedPtr<FJsonObject> ValuePropObject = PropertyJsonObject->GetObjectField(TEXT("ValueProp"));
            FEdGraphPinType ResolvedTerminalType = ResolvePropertyPinType(ValuePropObject);
            
            ResolvedType.PinValueType.TerminalCategory = ResolvedTerminalType.PinCategory;
            ResolvedType.PinValueType.TerminalSubCategory = ResolvedTerminalType.PinSubCategory;
            ResolvedType.PinValueType.TerminalSubCategoryObject = ResolvedTerminalType.PinSubCategoryObject;

            return ResolvedType;
        }

        if (*ContainerType == EPinContainerType::Set) {
            TSharedPtr<FJsonObject> ElementPropObject = PropertyJsonObject->GetObjectField(TEXT("ElementProp"));
            FEdGraphPinType ResolvedType = ResolvePropertyPinType(ElementPropObject);
            
            ResolvedType.ContainerType = *ContainerType;
            
            return ResolvedType;
        }

        if (*ContainerType == EPinContainerType::Array) {
            TSharedPtr<FJsonObject> InnerTypeObject = PropertyJsonObject->GetObjectField(TEXT("Inner"));
            FEdGraphPinType ResolvedType = ResolvePropertyPinType(InnerTypeObject);
            
            ResolvedType.ContainerType = *ContainerType;
            
            return ResolvedType;
        }
    }

    FEdGraphPinType ResolvedType = FEdGraphPinType(NAME_None, NAME_None, nullptr, EPinContainerType::None,false, FEdGraphTerminalType());

    /* Find main type from our PropertyCategoryMap */
    const FName* TypeCategory = PropertyCategoryMap.Find(Type);
    
    if (TypeCategory) {
        ResolvedType.PinCategory = *TypeCategory;
    } else {
        UE_LOG(LogJson, Warning, TEXT("Type '%s' not found in PropertyCategoryMap, defaulting to 'Byte'"), *Type);
        ResolvedType.PinCategory = TEXT("byte");
    }

    /* Special handling for some types */
    if (Type == "DoubleProperty") {
        ResolvedType.PinSubCategory = TEXT("double");
    } else if (Type == "FloatProperty") {
        ResolvedType.PinSubCategory = TEXT("float");
    } else if (Type == "EnumProperty" || Type == "ByteProperty") {
        ResolvedType.PinSubCategoryObject = LoadObjectFromJsonReference(PropertyJsonObject, TEXT("Enum"));
    } else if (Type == "StructProperty") {
        ResolvedType.PinSubCategoryObject = LoadObjectFromJsonReference(PropertyJsonObject, TEXT("Struct"));
    } else if (Type == "ClassProperty" || Type == "SoftClassProperty") {
        ResolvedType.PinSubCategoryObject = LoadObjectFromJsonReference(PropertyJsonObject, TEXT("MetaClass"));
    } else if (Type == "ObjectProperty" || Type == "SoftObjectProperty") {
        ResolvedType.PinSubCategoryObject = LoadObjectFromJsonReference(PropertyJsonObject, TEXT("PropertyClass"));
    }

    return ResolvedType;
}

UObject* IUserDefinedStructImporter::LoadObjectFromJsonReference(const TSharedPtr<FJsonObject> &ParentJsonObject, const FString &ReferenceKey) {
    const TSharedPtr<FJsonObject> ReferenceObject = ParentJsonObject->GetObjectField(ReferenceKey);
    
    if (!ReferenceObject) {
        UE_LOG(LogJson, Error, TEXT("Failed to load Object from property %s: property not found"), *ReferenceKey);
        return nullptr;
    }

    TObjectPtr<UObject> LoadedObject;
    LoadObject<UObject>(&ReferenceObject, LoadedObject);
    return LoadedObject;
}
