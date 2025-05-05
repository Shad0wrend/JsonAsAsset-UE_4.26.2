/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "Modules/LocalFetchModule.h"

#include "Interfaces/IPluginManager.h"
#include "Settings/JsonAsAssetSettings.h"
#include "Windows/WindowsHWrapper.h"
#include "Utilities/Compatibility.h"
#include "Utilities/EngineUtilities.h"

#ifdef _MSC_VER
#undef GetObject
#endif

bool LocalFetchModule::LaunchLocalFetch() {
	const UJsonAsAssetSettings* Settings = GetMutableDefault<UJsonAsAssetSettings>();

	FString PluginFolder; {
		const TSharedPtr<IPlugin> PluginInfo = IPluginManager::Get().FindPlugin("JsonAsAsset");

		if (PluginInfo.IsValid()) {
			PluginFolder = PluginInfo->GetBaseDir();
		}
	}

	FString FullPath = FPaths::ConvertRelativePathToFull(PluginFolder + "/Dependencies/LocalFetch/Release/Win64/LocalFetch.exe");
	FString Params = "--urls=" + Settings->LocalFetchUrl;

#if ENGINE_UE5
	return FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath, *Params, ELaunchVerb::Open);
#else
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath, *Params, ELaunchVerb::Open);
	
	return IsProcessRunning("LocalFetch.exe");
#endif
}

void LocalFetchModule::CloseLocalFetch() {
	CloseApplicationByProcessName("LocalFetch.exe");
}

bool LocalFetchModule::IsSetup(const UJsonAsAssetSettings* Settings, TArray<FString>& Params) {
	if (!Settings->bEnableLocalFetch) {
		return true;
	}
	
	if (Settings->MappingFilePath.FilePath.IsEmpty()) {
		Params.Add("Mappings file is missing");
	}

	if (Settings->ArchiveDirectory.Path.IsEmpty()) {
		Params.Add("Archive directory is missing");
	}

	return !(Settings->MappingFilePath.FilePath.IsEmpty() || Settings->ArchiveDirectory.Path.IsEmpty());
}

bool LocalFetchModule::IsSetup(const UJsonAsAssetSettings* Settings) {
	TArray<FString> Params;
	return IsSetup(Settings, Params);
}
