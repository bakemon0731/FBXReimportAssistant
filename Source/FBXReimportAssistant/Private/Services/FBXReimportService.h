#pragma once

#include "CoreMinimal.h"
#include "FBXReimportTypes.h"

class UStaticMesh;

// UIに依存しない、FBX検証・設定変更・再インポート処理。
class FFBXReimportService
{
public:
    using FOnReimportCompleted = TFunction<void(bool, int32, const FString&)>;

    // Static Meshがこのツールの対応対象か検証し、編集用データを作成する。
    static bool TryCreateDraft(
        UStaticMesh* StaticMesh,
        TSharedPtr<FFBXImportSettingsDraft>& OutDraft,
        FString& OutError
    );

    // UIで編集された設定値をInterchange Pipelineに適用する。
    static bool ApplyDraftSettings(
        const TSharedPtr<FFBXImportSettingsDraft>& Draft,
        FString& OutMessage
    );

    // 複数アセットへ共通設定を一括適用する。1回のUndoで全対象を戻せる。
    static bool ApplyBatchOverrides(
        const TSharedPtr<FFBXBatchState>& BatchState,
        FString& OutMessage
    );

    // 既存アセットを非同期で再インポートする。
    static bool StartReimportAsync(
        const TSharedPtr<FFBXImportSettingsDraft>& Draft,
        FOnReimportCompleted OnCompleted,
        FString& OutError
    );

    // 検証済みアセットを順番に再インポートする。
    static void StartBatch(
        const TSharedPtr<FFBXBatchState>& BatchState,
        bool bRetryFailedOnly = false
    );

    // 現在処理中のアセットが完了した後、次のアセットへ進まないよう要求する。
    static void RequestCancelBatch(const TSharedPtr<FFBXBatchState>& BatchState);

private:
    static void ProcessNextBatchItem(
        const TSharedPtr<FFBXBatchState>& BatchState
    );
};
