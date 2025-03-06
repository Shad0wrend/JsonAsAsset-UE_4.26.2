// Copyright JAA Contributors 2024-2025

#include "Modules/LocalFetchModule.h"

#include "Interfaces/IPluginManager.h"
#include "Settings/JsonAsAssetSettings.h"
#include "Windows/WindowsHWrapper.h"
#include <TlHelp32.h>

#ifdef _MSC_VER
#undef GetObject
#endif

/**
 * A list of classes allowed to be used by Local Fetch
 */
TArray<FString> LocalFetchAcceptedTypes = {
	"Texture2D",
	// "TextureCube",
	// "VolumeTexture",
	"TextureRenderTarget2D",

	"", // separator

	"Material",
	"MaterialInterface",
	"MaterialFunction",
	"MaterialInstanceConstant",
	"MaterialParameterCollection",
	"NiagaraParameterCollection",

	"", // separator

	"CurveFloat",
	"CurveTable",
	"CurveVector",
	"CurveLinearColorAtlas",
	"CurveLinearColor",

	"", // separator

	"SoundWave",
	"SoundCue",
	"ReverbEffect",
	"SoundAttenuation",
	"SoundConcurrency",
	"SoundClass",
	"SoundMix",
	"SoundModulationPatch",
		
	"", // separator

	"PhysicalMaterial",
	"SubsurfaceProfile",
	"LandscapeGrassType",
	"DataTable",
};

bool LocalFetchModule::LaunchLocalFetch()
{
	const UJsonAsAssetSettings* Settings = GetMutableDefault<UJsonAsAssetSettings>();

	FString PluginFolder;

	const TSharedPtr<IPlugin> PluginInfo = IPluginManager::Get().FindPlugin("JsonAsAsset");
	
	if (PluginInfo.IsValid()) {
		PluginFolder = PluginInfo->GetBaseDir();
	}

	FString FullPath = FPaths::ConvertRelativePathToFull(PluginFolder + "/Dependencies/LocalFetch/Release/Win64/LocalFetch.exe");
	FString Params = "--urls=" + Settings->LocalFetchUrl;
	
	return FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath, *Params, ELaunchVerb::Open);
}

void LocalFetchModule::CloseLocalFetch()
{
	FString ProcessName = TEXT("LocalFetch.exe");
	TCHAR* ProcessNameChar = ProcessName.GetCharArray().GetData();

	DWORD ProcessID = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 ProcessEntry;
		ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

		if (Process32First(hSnapshot, &ProcessEntry)) {
			do {
				if (FCString::Stricmp(ProcessEntry.szExeFile, ProcessNameChar) == 0) {
					ProcessID = ProcessEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(hSnapshot, &ProcessEntry));
		}
		CloseHandle(hSnapshot);
	}

	if (ProcessID != 0) {
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, ProcessID);
		if (hProcess != nullptr) {
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}
	}
}
