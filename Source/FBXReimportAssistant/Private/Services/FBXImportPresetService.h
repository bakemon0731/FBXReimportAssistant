#pragma once

#include "CoreMinimal.h"
#include "FBXReimportTypes.h"

// プリセットに関する処理を担当

class FBXImportPresetService
{
public:
	
	static bool SavePreset(
		const FFBXImportPreset& Preset,
		bool bOverwriteExisting,
		FString& OutFilePath,
		FString& OutMessage);
	
	static bool LoadPreset(
		const FString& FilePath,
		FFBXImportPreset& OutPreset,
		FString& OutMessage);
	
	static bool GetPresetFiles(
		TArray<FString>& OutFilePaths,
		FString& OutMessage);
	
	static bool DeletePreset(
		const FString& PresetName,
		FString& OutMessage);
	
};
