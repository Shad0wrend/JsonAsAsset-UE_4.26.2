/* Copyright JAA Contributors 2024-2025 */

#include "Importers/Types/Audio/SoundCueImporter.h"
#include "Sound/SoundCue.h"

bool ISoundCueImporter::Import() {
	/* Create Sound Cue */
	USoundCue* SoundCue = NewObject<USoundCue>(Package, *FileName, RF_Public | RF_Standalone);
	SoundCue->PreEditChange(nullptr);

	TSharedPtr<FJsonObject> Properties = JsonObject->GetObjectField(TEXT("Properties"));
	
	/* Start importing nodes ~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	if (SoundCue) {
		TMap<FString, USoundNode*> SoundCueNodes;
		
		ConstructNodes(SoundCue, AllJsonObjects, SoundCueNodes);
		SetupNodes(SoundCue, SoundCueNodes, AllJsonObjects);
	}
	/* End of importing nodes ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

	GetObjectSerializer()->DeserializeObjectProperties(RemovePropertiesShared(Properties, TArray<FString>
	{
		"FirstNode"
	}), SoundCue);
	
	SoundCue->PostEditChange();
	SoundCue->CompileSoundNodesFromGraphNodes();

	return OnAssetCreation(SoundCue);
}
