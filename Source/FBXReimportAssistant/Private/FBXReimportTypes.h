#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "InterchangeAssetImportData.h"

// 1つのStatic Meshについて、UIで編集・表示する情報。
struct FFBXImportSettingsDraft
{
    FString AssetName;
    FString SourceFilePath;

    FVector Translation = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float UniformScale = 1.0f;

    TWeakObjectPtr<UStaticMesh> TargetMesh;
    TWeakObjectPtr<UInterchangeAssetImportData> ImportData;

    bool bSettingsApplied = true;
    bool bReimportInProgress = false;
};

// 複数アセットに共通して適用する上書き設定。
// チェックされていない項目は、各アセットの元の値を維持する。
struct FFBXBatchOverrideSettings
{
    bool bOverrideTranslation = false;
    FVector Translation = FVector::ZeroVector;

    bool bOverrideRotation = false;
    FRotator Rotation = FRotator::ZeroRotator;

    bool bOverrideUniformScale = false;
    float UniformScale = 1.0f;

    bool HasAnyOverride() const
    {
        return bOverrideTranslation ||
            bOverrideRotation ||
            bOverrideUniformScale;
    }
};

// 一括処理における、1アセット分の結果。
struct FFBXBatchItem
{
    FString AssetName;
    FString SourceFilePath;
    FString Status;

    TSharedPtr<FFBXImportSettingsDraft> Draft;
    bool bEligible = false;

    // 一括設定適用前の値を保持する。
    // チェックを外した項目を元の値に戻すために使用する。
    FVector OriginalTranslation = FVector::ZeroVector;
    FRotator OriginalRotation = FRotator::ZeroRotator;
    float OriginalUniformScale = 1.0f;
};

// 一括再インポート全体の状態。
struct FFBXBatchState
{
    TArray<TSharedPtr<FFBXBatchItem>> Items;

    FFBXBatchOverrideSettings OverrideSettings;

    int32 CurrentIndex = 0;
    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    int32 SkippedCount = 0;

    bool bIsRunning = false;

    // UIで値を変更したが、まだ一括適用していない状態。
    bool bOverridesPending = false;

    // 以前に一括適用した値があり、現在すべてのチェックを外した場合でも
    // 元の値へ戻すため、もう一度適用が必要な状態を管理する。
    bool bBatchOverridesEverApplied = false;

    FString StatusText = TEXT("一括設定の準備ができました。");
};
