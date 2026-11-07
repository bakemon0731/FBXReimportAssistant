#include "Services/FBXReimportService.h"

#include "Engine/StaticMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeManager.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Async/Async.h"

namespace
{
    bool IsSupportedFbxSource(const FString& FilePath, FString& OutError)
    {
        if (!FPaths::GetExtension(FilePath).Equals(TEXT("fbx"), ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT("FBX以外のソースファイルです。\n%s"),
                *FilePath
            );
            return false;
        }

        if (!FPaths::FileExists(FilePath))
        {
            OutError = FString::Printf(
                TEXT("元のFBXファイルが見つかりません。\n%s"),
                *FilePath
            );
            return false;
        }

        return true;
    }

    bool FindGenericAssetsPipeline(
        UInterchangeAssetImportData* ImportData,
        UInterchangeGenericAssetsPipeline*& OutPipeline,
        TArray<UObject*>& OutPipelines,
        FString& OutError
    )
    {
        OutPipeline = nullptr;
        OutPipelines.Reset();

        if (!IsValid(ImportData))
        {
            OutError = TEXT("Interchange Import Dataが無効です。");
            return false;
        }

        OutPipelines = ImportData->GetPipelines();

        for (UObject* PipelineObject : OutPipelines)
        {
            if (!IsValid(PipelineObject))
            {
                continue;
            }

            OutPipeline = Cast<UInterchangeGenericAssetsPipeline>(PipelineObject);
            if (OutPipeline != nullptr)
            {
                return true;
            }
        }

        OutError = FString::Printf(
            TEXT("Generic Assets Pipelineが見つかりません。保存Pipeline数: %d"),
            OutPipelines.Num()
        );
        return false;
    }
}

bool FFBXReimportService::TryCreateDraft(
    UStaticMesh* StaticMesh,
    TSharedPtr<FFBXImportSettingsDraft>& OutDraft,
    FString& OutError
)
{
    OutDraft.Reset();
    OutError.Reset();

    if (!IsValid(StaticMesh))
    {
        OutError = TEXT("Static Meshが無効です。");
        return false;
    }

    UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();

    if (Manager.IsObjectBeingImported(StaticMesh))
    {
        OutError = FString::Printf(
            TEXT("アセットは現在インポート中です。\n%s"),
            *StaticMesh->GetName()
        );
        return false;
    }

    TArray<FString> SourceFiles;
    if (!Manager.CanReimport(StaticMesh, SourceFiles))
    {
        OutError = FString::Printf(
            TEXT("このメッシュは再インポート対象外です。\nアセット: %s\nFBXソース情報がないか、Interchangeが再インポートに対応していません。"),
            *StaticMesh->GetName()
        );
        return false;
    }

    if (SourceFiles.Num() != 1)
    {
        OutError = FString::Printf(
            TEXT("このツールはソースファイルが1つのアセットに対応しています。\n検出数: %d"),
            SourceFiles.Num()
        );
        return false;
    }

    const FString SourceFilePath = SourceFiles[0];
    if (!IsSupportedFbxSource(SourceFilePath, OutError))
    {
        return false;
    }

    UAssetImportData* BaseImportData = StaticMesh->AssetImportData.Get();
    if (!IsValid(BaseImportData))
    {
        OutError = TEXT("AssetImportDataが存在しません。");
        return false;
    }

    UInterchangeAssetImportData* InterchangeImportData =
        Cast<UInterchangeAssetImportData>(BaseImportData);

    if (!IsValid(InterchangeImportData))
    {
        OutError = FString::Printf(
            TEXT("Interchange形式のImport Dataではありません。実際の型: %s"),
            *BaseImportData->GetClass()->GetName()
        );
        return false;
    }

    TArray<UObject*> Pipelines;
    UInterchangeGenericAssetsPipeline* GenericPipeline = nullptr;

    if (!FindGenericAssetsPipeline(
        InterchangeImportData,
        GenericPipeline,
        Pipelines,
        OutError))
    {
        return false;
    }

    OutDraft = MakeShared<FFBXImportSettingsDraft>();
    OutDraft->AssetName = StaticMesh->GetName();
    OutDraft->SourceFilePath = SourceFilePath;
    OutDraft->Translation = GenericPipeline->ImportOffsetTranslation;
    OutDraft->Rotation = GenericPipeline->ImportOffsetRotation;
    OutDraft->UniformScale = GenericPipeline->ImportOffsetUniformScale;
    OutDraft->TargetMesh = StaticMesh;
    OutDraft->ImportData = InterchangeImportData;
    OutDraft->bSettingsApplied = true;
    OutDraft->bReimportInProgress = false;

    return true;
}

bool FFBXReimportService::ApplyDraftSettings(
    const TSharedPtr<FFBXImportSettingsDraft>& Draft,
    FString& OutMessage
)
{
    OutMessage.Reset();

    if (!Draft.IsValid())
    {
        OutMessage = TEXT("編集データが存在しません。");
        return false;
    }

    UStaticMesh* TargetMesh = Draft->TargetMesh.Get();
    UInterchangeAssetImportData* ImportData = Draft->ImportData.Get();

    if (!IsValid(TargetMesh) || !IsValid(ImportData))
    {
        OutMessage = TEXT("適用対象のアセットまたはImport Dataが無効です。");
        return false;
    }

    if (TargetMesh->AssetImportData.Get() != ImportData)
    {
        OutMessage = TEXT("アセットのImport Dataが変更されています。ウィンドウを開き直してください。");
        return false;
    }

    const bool bTranslationValid =
        FMath::IsFinite(Draft->Translation.X) &&
        FMath::IsFinite(Draft->Translation.Y) &&
        FMath::IsFinite(Draft->Translation.Z);

    const bool bRotationValid =
        FMath::IsFinite(Draft->Rotation.Roll) &&
        FMath::IsFinite(Draft->Rotation.Pitch) &&
        FMath::IsFinite(Draft->Rotation.Yaw);

    const bool bScaleValid =
        FMath::IsFinite(Draft->UniformScale) &&
        !FMath::IsNearlyZero(Draft->UniformScale);

    if (!bTranslationValid || !bRotationValid || !bScaleValid)
    {
        OutMessage = TEXT("入力値が不正です。Uniform Scaleには0を指定できません。");
        return false;
    }

    TArray<UObject*> Pipelines;
    UInterchangeGenericAssetsPipeline* TargetPipeline = nullptr;

    if (!FindGenericAssetsPipeline(ImportData, TargetPipeline, Pipelines, OutMessage))
    {
        return false;
    }

    FScopedTransaction Transaction(
        FText::FromString(TEXT("FBX Reimport Assistant: Apply Import Settings")),
        true
    );

    TargetMesh->SetFlags(RF_Transactional);
    ImportData->SetFlags(RF_Transactional);
    TargetPipeline->SetFlags(RF_Transactional);

    const bool bMeshModified = TargetMesh->Modify();
    const bool bImportDataModified = ImportData->Modify();
    const bool bPipelineModified = TargetPipeline->Modify();

    if (!bMeshModified || !bImportDataModified || !bPipelineModified)
    {
        Transaction.Cancel();
        OutMessage = FString::Printf(
            TEXT("Undo対象の登録に失敗しました。\nMesh: %s\nImportData: %s\nPipeline: %s"),
            bMeshModified ? TEXT("OK") : TEXT("NG"),
            bImportDataModified ? TEXT("OK") : TEXT("NG"),
            bPipelineModified ? TEXT("OK") : TEXT("NG")
        );
        return false;
    }

    TargetPipeline->ImportOffsetTranslation = Draft->Translation;
    TargetPipeline->ImportOffsetRotation = Draft->Rotation;
    TargetPipeline->ImportOffsetUniformScale = Draft->UniformScale;

    ImportData->SetPipelines(Pipelines);
    TargetMesh->MarkPackageDirty();

    Draft->bSettingsApplied = true;

    OutMessage = FString::Printf(
        TEXT("設定を適用しました。\nアセット: %s\n\nTranslation: %.2f / %.2f / %.2f\nRotation: %.2f / %.2f / %.2f\nUniform Scale: %.2f\n\nアセットを保存してください。再インポートはまだ実行していません。"),
        *Draft->AssetName,
        Draft->Translation.X,
        Draft->Translation.Y,
        Draft->Translation.Z,
        Draft->Rotation.Roll,
        Draft->Rotation.Pitch,
        Draft->Rotation.Yaw,
        Draft->UniformScale
    );

    return true;
}

bool FFBXReimportService::ApplyBatchOverrides(
    const TSharedPtr<FFBXBatchState>& BatchState,
    FString& OutMessage
)
{
    OutMessage.Reset();

    if (!BatchState.IsValid())
    {
        OutMessage = TEXT("一括処理データが存在しません。");
        return false;
    }

    if (BatchState->bIsRunning)
    {
        OutMessage = TEXT("一括再インポート中は設定を変更できません。");
        return false;
    }

    const FFBXBatchOverrideSettings& Overrides = BatchState->OverrideSettings;

    if (!Overrides.HasAnyOverride() && !BatchState->bBatchOverridesEverApplied)
    {
        OutMessage = TEXT("一括変更する項目にチェックを入れてください。");
        return false;
    }

    if (Overrides.bOverrideTranslation)
    {
        if (!FMath::IsFinite(Overrides.Translation.X) ||
            !FMath::IsFinite(Overrides.Translation.Y) ||
            !FMath::IsFinite(Overrides.Translation.Z))
        {
            OutMessage = TEXT("Translationに不正な数値が入力されています。");
            return false;
        }
    }

    if (Overrides.bOverrideRotation)
    {
        if (!FMath::IsFinite(Overrides.Rotation.Roll) ||
            !FMath::IsFinite(Overrides.Rotation.Pitch) ||
            !FMath::IsFinite(Overrides.Rotation.Yaw))
        {
            OutMessage = TEXT("Rotationに不正な数値が入力されています。");
            return false;
        }
    }

    if (Overrides.bOverrideUniformScale)
    {
        if (!FMath::IsFinite(Overrides.UniformScale) ||
            FMath::IsNearlyZero(Overrides.UniformScale))
        {
            OutMessage = TEXT("Uniform Scaleには有限値かつ0以外を指定してください。");
            return false;
        }
    }

    // 変更前に全対象を検証する。
    // 途中まで適用してから失敗する部分適用を防ぐ。
    struct FBatchApplyRecord
    {
        TSharedPtr<FFBXBatchItem> Item;
        UStaticMesh* Mesh = nullptr;
        UInterchangeAssetImportData* ImportData = nullptr;
        UInterchangeGenericAssetsPipeline* Pipeline = nullptr;
        TArray<UObject*> Pipelines;
        FVector NewTranslation = FVector::ZeroVector;
        FRotator NewRotation = FRotator::ZeroRotator;
        float NewUniformScale = 1.0f;
    };

    TArray<FBatchApplyRecord> Records;
    TSet<UStaticMesh*> SeenMeshes;

    for (const TSharedPtr<FFBXBatchItem>& Item : BatchState->Items)
    {
        if (!Item.IsValid() || !Item->bEligible)
        {
            continue;
        }

        if (!Item->Draft.IsValid())
        {
            OutMessage = FString::Printf(
                TEXT("%s: 編集データが存在しません。"),
                *Item->AssetName
            );
            return false;
        }

        UStaticMesh* Mesh = Item->Draft->TargetMesh.Get();
        UInterchangeAssetImportData* ImportData = Item->Draft->ImportData.Get();

        if (!IsValid(Mesh) || !IsValid(ImportData))
        {
            OutMessage = FString::Printf(
                TEXT("%s: アセットまたはImport Dataが無効です。"),
                *Item->AssetName
            );
            return false;
        }

        if (Mesh->AssetImportData.Get() != ImportData)
        {
            OutMessage = FString::Printf(
                TEXT("%s: Import Dataが変更されています。ウィンドウを開き直してください。"),
                *Item->AssetName
            );
            return false;
        }

        // ウィンドウを開いた後に状態が変わっていないか再検証する。
        UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
        if (Manager.IsObjectBeingImported(Mesh))
        {
            OutMessage = FString::Printf(
                TEXT("%s: 現在インポート中のため設定を適用できません。"),
                *Item->AssetName
            );
            return false;
        }

        TArray<FString> CurrentSourceFiles;
        if (!Manager.CanReimport(Mesh, CurrentSourceFiles) || CurrentSourceFiles.Num() != 1)
        {
            OutMessage = FString::Printf(
                TEXT("%s: 現在の状態ではFBX再インポート対象ではありません。"),
                *Item->AssetName
            );
            return false;
        }

        FString SourceValidationError;
        if (!IsSupportedFbxSource(CurrentSourceFiles[0], SourceValidationError))
        {
            OutMessage = FString::Printf(
                TEXT("%s: %s"),
                *Item->AssetName,
                *SourceValidationError
            );
            return false;
        }

        if (!FPaths::ConvertRelativePathToFull(CurrentSourceFiles[0]).Equals(
            FPaths::ConvertRelativePathToFull(Item->SourceFilePath),
            ESearchCase::IgnoreCase))
        {
            OutMessage = FString::Printf(
                TEXT("%s: FBXソースパスが変更されています。ウィンドウを開き直してください。"),
                *Item->AssetName
            );
            return false;
        }

        if (SeenMeshes.Contains(Mesh))
        {
            continue;
        }
        SeenMeshes.Add(Mesh);

        TArray<UObject*> Pipelines = ImportData->GetPipelines();
        UInterchangeGenericAssetsPipeline* Pipeline = nullptr;

        for (UObject* PipelineObject : Pipelines)
        {
            if (!IsValid(PipelineObject))
            {
                continue;
            }

            Pipeline = Cast<UInterchangeGenericAssetsPipeline>(PipelineObject);
            if (Pipeline != nullptr)
            {
                break;
            }
        }

        if (!IsValid(Pipeline))
        {
            OutMessage = FString::Printf(
                TEXT("%s: Generic Assets Pipelineが見つかりません。"),
                *Item->AssetName
            );
            return false;
        }

        FBatchApplyRecord& Record = Records.AddDefaulted_GetRef();
        Record.Item = Item;
        Record.Mesh = Mesh;
        Record.ImportData = ImportData;
        Record.Pipeline = Pipeline;
        Record.Pipelines = MoveTemp(Pipelines);

        // チェックされた項目は共通値で上書き。
        // チェックされていない項目は、各アセットの初期値に戻す。
        Record.NewTranslation = Overrides.bOverrideTranslation
            ? Overrides.Translation
            : Item->OriginalTranslation;

        Record.NewRotation = Overrides.bOverrideRotation
            ? Overrides.Rotation
            : Item->OriginalRotation;

        Record.NewUniformScale = Overrides.bOverrideUniformScale
            ? Overrides.UniformScale
            : Item->OriginalUniformScale;
    }

    if (Records.IsEmpty())
    {
        OutMessage = TEXT("一括設定を適用できるアセットがありません。");
        return false;
    }

    // 全対象を1つのトランザクションにまとめる。
    FScopedTransaction Transaction(
        FText::FromString(TEXT("FBX Reimport Assistant: Apply Batch Import Settings")),
        true
    );

    // まず全オブジェクトをUndo対象に登録する。値の変更はまだ行わない。
    for (FBatchApplyRecord& Record : Records)
    {
        Record.Mesh->SetFlags(RF_Transactional);
        Record.ImportData->SetFlags(RF_Transactional);
        Record.Pipeline->SetFlags(RF_Transactional);

        const bool bMeshModified = Record.Mesh->Modify();
        const bool bImportDataModified = Record.ImportData->Modify();
        const bool bPipelineModified = Record.Pipeline->Modify();

        if (!bMeshModified || !bImportDataModified || !bPipelineModified)
        {
            Transaction.Cancel();
            OutMessage = FString::Printf(
                TEXT("%s: Undo対象の登録に失敗しました。Mesh=%s / ImportData=%s / Pipeline=%s"),
                *Record.Item->AssetName,
                bMeshModified ? TEXT("OK") : TEXT("NG"),
                bImportDataModified ? TEXT("OK") : TEXT("NG"),
                bPipelineModified ? TEXT("OK") : TEXT("NG")
            );
            return false;
        }
    }

    // すべての検証とUndo登録が終わってから実際の値を変更する。
    for (FBatchApplyRecord& Record : Records)
    {
        Record.Pipeline->ImportOffsetTranslation = Record.NewTranslation;
        Record.Pipeline->ImportOffsetRotation = Record.NewRotation;
        Record.Pipeline->ImportOffsetUniformScale = Record.NewUniformScale;

        Record.ImportData->SetPipelines(Record.Pipelines);
        Record.Mesh->MarkPackageDirty();

        // UI・再インポート処理が使うDraftも同期する。
        Record.Item->Draft->Translation = Record.NewTranslation;
        Record.Item->Draft->Rotation = Record.NewRotation;
        Record.Item->Draft->UniformScale = Record.NewUniformScale;
        Record.Item->Draft->bSettingsApplied = true;
        Record.Item->Status = TEXT("設定適用済み・再インポート待機中");
    }

    BatchState->bOverridesPending = false;
    BatchState->bBatchOverridesEverApplied = Overrides.HasAnyOverride();

    OutMessage = FString::Printf(
        TEXT("一括設定を適用しました。\n対象アセット数: %d\n\nチェックした項目は共通値に変更し、チェックしていない項目は各アセットの元の値を維持しました。\n\nアセットを保存してから再インポートしてください。"),
        Records.Num()
    );

    BatchState->StatusText = OutMessage;
    return true;
}

bool FFBXReimportService::StartReimportAsync(
    const TSharedPtr<FFBXImportSettingsDraft>& Draft,
    FOnReimportCompleted OnCompleted,
    FString& OutError
)
{
    OutError.Reset();

    if (!Draft.IsValid())
    {
        OutError = TEXT("編集データが存在しません。");
        return false;
    }

    if (!Draft->bSettingsApplied)
    {
        OutError = TEXT("未適用の設定があります。先に設定を適用してください。");
        return false;
    }

    if (Draft->bReimportInProgress)
    {
        OutError = TEXT("このアセットは既に再インポート中です。");
        return false;
    }

    UStaticMesh* TargetMesh = Draft->TargetMesh.Get();
    UInterchangeAssetImportData* ImportData = Draft->ImportData.Get();

    if (!IsValid(TargetMesh) || !IsValid(ImportData))
    {
        OutError = TEXT("対象アセットまたはImport Dataが無効です。");
        return false;
    }

    if (TargetMesh->AssetImportData.Get() != ImportData)
    {
        OutError = TEXT("Import Dataが変更されています。ウィンドウを開き直してください。");
        return false;
    }

    UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();

    if (Manager.IsObjectBeingImported(TargetMesh))
    {
        OutError = TEXT("このアセットは現在インポート中です。");
        return false;
    }

    TArray<FString> SourceFiles;
    if (!Manager.CanReimport(TargetMesh, SourceFiles) || SourceFiles.Num() != 1)
    {
        OutError = TEXT("このアセットは再インポート対象外です。");
        return false;
    }

    FString SourceError;
    if (!IsSupportedFbxSource(SourceFiles[0], SourceError))
    {
        OutError = SourceError;
        return false;
    }

    if (!FPaths::ConvertRelativePathToFull(SourceFiles[0]).Equals(
        FPaths::ConvertRelativePathToFull(Draft->SourceFilePath),
        ESearchCase::IgnoreCase))
    {
        OutError = TEXT("FBXソースパスが変更されています。ウィンドウを開き直してください。");
        return false;
    }

    FImportAssetParameters Parameters;
    Parameters.bIsAutomated = true;
    Parameters.bReplaceExisting = true;
    Parameters.ReimportSourceIndex = 0;

    UE::Interchange::FAssetImportResultRef ImportResult =
        Manager.ReimportAssetAsync(TargetMesh, Parameters);

    if (!ImportResult->IsValid())
    {
        OutError = TEXT("Interchangeの再インポート処理を開始できませんでした。");
        return false;
    }

    Draft->bReimportInProgress = true;

    TSharedRef<FOnReimportCompleted> Completion =
        MakeShared<FOnReimportCompleted>(MoveTemp(OnCompleted));

    ImportResult->OnDone(
        [Completion](UE::Interchange::FImportResult& Result)
        {
            const bool bSuccess =
                Result.GetStatus() == UE::Interchange::FImportResult::EStatus::Done &&
                Result.GetImportedObjects().Num() > 0;

            const int32 ImportedObjectCount = Result.GetImportedObjects().Num();

            const FString ResultMessage = bSuccess
                ? TEXT("再インポートが完了しました。メッシュを確認して保存してください。")
                : TEXT("再インポートが正常に完了しませんでした。Output Logを確認してください。");

            AsyncTask(
                ENamedThreads::GameThread,
                [Completion, bSuccess, ImportedObjectCount, ResultMessage]()
                {
                    if (*Completion)
                    {
                        (*Completion)(bSuccess, ImportedObjectCount, ResultMessage);
                    }
                }
            );
        }
    );

    return true;
}

namespace
{
    void AddBatchLog(const TSharedPtr<FFBXBatchState>& BatchState, const FString& Message)
    {
        if (!BatchState.IsValid())
        {
            return;
        }

        BatchState->LogMessages.Add(FString::Printf(
            TEXT("[%s] %s"),
            *FDateTime::Now().ToString(TEXT("%H:%M:%S")),
            *Message
        ));
    }
}

void FFBXReimportService::StartBatch(
    const TSharedPtr<FFBXBatchState>& BatchState,
    bool bRetryFailedOnly
)
{
    if (!BatchState.IsValid() || BatchState->bIsRunning)
    {
        return;
    }

    if (BatchState->bOverridesPending)
    {
        BatchState->StatusText = TEXT("一括設定に未適用の変更があります。先に「一括設定を適用」を実行してください。");
        AddBatchLog(BatchState, BatchState->StatusText);
        return;
    }

    BatchState->CurrentIndex = 0;
    BatchState->SuccessCount = 0;
    BatchState->FailedCount = 0;
    BatchState->SkippedCount = 0;
    BatchState->PendingCount = 0;
    BatchState->ProcessedCount = 0;
    BatchState->bCancelRequested = false;

    for (const TSharedPtr<FFBXBatchItem>& Item : BatchState->Items)
    {
        if (!Item.IsValid())
        {
            continue;
        }

        Item->bPendingForBatch = false;

        if (!Item->bEligible || !Item->Draft.IsValid())
        {
            if (!bRetryFailedOnly)
            {
                ++BatchState->SkippedCount;
            }
            continue;
        }

        const bool bIsFailedItem = Item->Status.StartsWith(TEXT("失敗"));
        const bool bShouldRun = !bRetryFailedOnly || bIsFailedItem;

        if (!bShouldRun)
        {
            continue;
        }

        Item->bPendingForBatch = true;
        Item->Status = TEXT("待機中");

        // 前回の完了処理では二重実行防止のためfalseにしている。
        // 保存済みPipeline設定を使って再実行できるように戻す。
        Item->Draft->bSettingsApplied = true;
        Item->Draft->bReimportInProgress = false;

        ++BatchState->PendingCount;
    }

    if (BatchState->PendingCount == 0)
    {
        BatchState->StatusText = bRetryFailedOnly
            ? TEXT("再試行できる失敗アセットがありません。")
            : TEXT("再インポート可能なアセットがありません。");
        AddBatchLog(BatchState, BatchState->StatusText);
        return;
    }

    BatchState->bIsRunning = true;
    BatchState->StatusText = bRetryFailedOnly
        ? FString::Printf(TEXT("失敗したアセット %d 件の再試行を開始しました。"), BatchState->PendingCount)
        : FString::Printf(TEXT("一括再インポートを開始しました。対象: %d 件"), BatchState->PendingCount);

    AddBatchLog(BatchState, BatchState->StatusText);
    ProcessNextBatchItem(BatchState);
}

void FFBXReimportService::RequestCancelBatch(
    const TSharedPtr<FFBXBatchState>& BatchState
)
{
    if (!BatchState.IsValid() || !BatchState->bIsRunning || BatchState->bCancelRequested)
    {
        return;
    }

    BatchState->bCancelRequested = true;
    BatchState->StatusText = TEXT("中断要求を受け付けました。現在処理中のアセットが完了した後に停止します。");
    AddBatchLog(BatchState, BatchState->StatusText);
}

void FFBXReimportService::ProcessNextBatchItem(
    const TSharedPtr<FFBXBatchState>& BatchState
)
{
    if (!BatchState.IsValid() || !BatchState->bIsRunning)
    {
        return;
    }

    // Interchangeの非同期インポート自体を強制停止するのではなく、
    // 現在の1件が終わったタイミングでキューの続行を止める。
    if (BatchState->bCancelRequested)
    {
        BatchState->bIsRunning = false;
        BatchState->StatusText = FString::Printf(
            TEXT("一括再インポートを中断しました。\n処理済み: %d / %d\n成功: %d\n失敗: %d\n未処理: %d"),
            BatchState->ProcessedCount,
            BatchState->PendingCount,
            BatchState->SuccessCount,
            BatchState->FailedCount,
            BatchState->PendingCount - BatchState->ProcessedCount
        );
        AddBatchLog(BatchState, BatchState->StatusText);
        return;
    }

    while (BatchState->CurrentIndex < BatchState->Items.Num())
    {
        const int32 ItemIndex = BatchState->CurrentIndex;
        const TSharedPtr<FFBXBatchItem> Item = BatchState->Items[ItemIndex];

        if (!Item.IsValid() || !Item->bPendingForBatch)
        {
            ++BatchState->CurrentIndex;
            continue;
        }

        Item->bPendingForBatch = false;

        if (!Item->bEligible || !Item->Draft.IsValid())
        {
            Item->Status = TEXT("スキップ: 有効な編集データがありません。");
            ++BatchState->SkippedCount;
            ++BatchState->ProcessedCount;
            ++BatchState->CurrentIndex;
            AddBatchLog(BatchState, FString::Printf(TEXT("%s: %s"), *Item->AssetName, *Item->Status));
            continue;
        }

        UStaticMesh* TargetMesh = Item->Draft->TargetMesh.Get();
        UInterchangeAssetImportData* ImportData = Item->Draft->ImportData.Get();

        if (!IsValid(TargetMesh) || !IsValid(ImportData) ||
            TargetMesh->AssetImportData.Get() != ImportData)
        {
            Item->Status = TEXT("スキップ: アセットまたはImport Dataが無効・変更済みです。");
            ++BatchState->SkippedCount;
            ++BatchState->ProcessedCount;
            ++BatchState->CurrentIndex;
            AddBatchLog(BatchState, FString::Printf(TEXT("%s: %s"), *Item->AssetName, *Item->Status));
            continue;
        }

        Item->Status = TEXT("再インポート中...");
        BatchState->StatusText = FString::Printf(
            TEXT("処理中: %d / %d\n対象: %s"),
            BatchState->ProcessedCount + 1,
            BatchState->PendingCount,
            *Item->AssetName
        );
        AddBatchLog(BatchState, FString::Printf(TEXT("%s の再インポートを開始します。"), *Item->AssetName));

        FString StartError;
        const bool bStarted = StartReimportAsync(
            Item->Draft,
            [BatchState, Item, ItemIndex](bool bSuccess, int32 ImportedCount, const FString& Message)
            {
                if (bSuccess)
                {
                    Item->Status = FString::Printf(TEXT("完了: %d オブジェクト"), ImportedCount);
                    ++BatchState->SuccessCount;
                }
                else
                {
                    Item->Status = FString::Printf(TEXT("失敗: %s"), *Message);
                    ++BatchState->FailedCount;
                }

                Item->Draft->bReimportInProgress = false;
                Item->Draft->bSettingsApplied = false;
                ++BatchState->ProcessedCount;
                BatchState->CurrentIndex = ItemIndex + 1;

                AddBatchLog(BatchState, FString::Printf(TEXT("%s: %s"), *Item->AssetName, *Item->Status));
                ProcessNextBatchItem(BatchState);
            },
            StartError
        );

        if (!bStarted)
        {
            Item->Status = FString::Printf(TEXT("失敗: %s"), *StartError);
            ++BatchState->FailedCount;
            ++BatchState->ProcessedCount;
            ++BatchState->CurrentIndex;
            AddBatchLog(BatchState, FString::Printf(TEXT("%s: %s"), *Item->AssetName, *Item->Status));
            continue;
        }

        // 非同期処理の完了通知を待つ。次のアセットにはここで進まない。
        return;
    }

    BatchState->bIsRunning = false;
    BatchState->StatusText = FString::Printf(
        TEXT("一括再インポートが終了しました。\n成功: %d\n失敗: %d\nスキップ: %d\n処理済み: %d / %d"),
        BatchState->SuccessCount,
        BatchState->FailedCount,
        BatchState->SkippedCount,
        BatchState->ProcessedCount,
        BatchState->PendingCount
    );
    AddBatchLog(BatchState, BatchState->StatusText);
}
