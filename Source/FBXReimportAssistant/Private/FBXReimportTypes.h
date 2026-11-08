#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetData.h"
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

struct FFBXImportPreset
{
    // プリセットの名前
    FString Name;
    
    // データ形式のバージョン
    int32 Version = 1;
    
    // インポート設定
    FFBXBatchOverrideSettings Settings;
};

// 一括処理における、1アセット分の結果。
struct FFBXBatchItem
{
    FAssetData AssetData;
    FString AssetName;
    FString AssetObjectPath;
    FString SourceFilePath;
    FString Status;

    TSharedPtr<FFBXImportSettingsDraft> Draft;
    bool bEligible = false;
    bool bSelectedForBatch = false;
    bool bBatchOverrideAppliedToThisItem = false;

    // 一括設定適用前の値を保持する。
    // チェックを外した項目を元の値に戻すために使用する。
    FVector OriginalTranslation = FVector::ZeroVector;
    FRotator OriginalRotation = FRotator::ZeroRotator;
    float OriginalUniformScale = 1.0f;

    // 次回のバッチ実行で処理する対象かどうか。
    bool bPendingForBatch = false;
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

    // 今回の実行対象数と、そのうち処理が完了した数。
    int32 PendingCount = 0;
    int32 ProcessedCount = 0;

    bool bIsRunning = false;

    // 中断要求は現在のInterchange処理が終わった後に反映する。
    bool bCancelRequested = false;

    // UIに表示する実行ログ。
    TArray<FString> LogMessages;

    // UIで値を変更したが、まだ一括適用していない状態。
    bool bOverridesPending = false;

    // 以前に一括適用した値があり、現在すべてのチェックを外した場合でも
    // 元の値へ戻すため、もう一度適用が必要な状態を管理する。
    bool bBatchOverridesEverApplied = false;

    FString StatusText = TEXT("一括設定の準備ができました。");
};
