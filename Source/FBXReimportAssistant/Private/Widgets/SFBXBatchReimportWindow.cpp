#include "Widgets/SFBXBatchReimportWindow.h"

#include "Services/FBXReimportService.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Notifications/SProgressBar.h"

namespace
{
    TSharedRef<SVerticalBox> MakeBatchNumericField(
        const FText& Label,
        float InitialValue,
        TFunction<void(float)> OnValueChanged,
        TFunction<bool()> IsEnabled
    )
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f)
            [
                SNew(STextBlock)
                .Text(Label)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f)
            [
                SNew(SSpinBox<float>)
                .Value(InitialValue)
                .Delta(0.1f)
                .EnableWheel(false)
                .MinDesiredWidth(100.0f)
                .IsEnabled_Lambda([IsEnabled]()
                {
                    return IsEnabled();
                })
                .OnValueChanged_Lambda(
                    [OnValueChanged](float NewValue)
                    {
                        OnValueChanged(NewValue);
                    }
                )
            ];
    }

    void MarkBatchOverridesEdited(const TSharedPtr<FFBXBatchState>& State)
    {
        if (!State.IsValid())
        {
            return;
        }

        State->bOverridesPending =
            State->OverrideSettings.HasAnyOverride() ||
            State->bBatchOverridesEverApplied;

        State->StatusText = State->bOverridesPending
            ? TEXT("一括設定に未適用の変更があります。")
            : TEXT("一括変更は選択されていません。各アセットの元の設定を使用します。");
    }

    FString BuildBatchPreview(const TSharedPtr<FFBXBatchState>& State)
    {
        if (!State.IsValid())
        {
            return TEXT("一括設定データがありません。");
        }

        FString Result;

        for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
        {
            if (!Item.IsValid())
            {
                continue;
            }

            if (!Item->bEligible || !Item->Draft.IsValid())
            {
                Result += FString::Printf(
                    TEXT("[対象外] %s\n理由: %s\n\n"),
                    *Item->AssetName,
                    *Item->Status
                );
                continue;
            }

            const FFBXBatchOverrideSettings& Overrides = State->OverrideSettings;
            const FVector NewTranslation = Overrides.bOverrideTranslation ? Overrides.Translation : Item->OriginalTranslation;
            const FRotator NewRotation = Overrides.bOverrideRotation ? Overrides.Rotation : Item->OriginalRotation;
            const float NewScale = Overrides.bOverrideUniformScale ? Overrides.UniformScale : Item->OriginalUniformScale;

            Result += FString::Printf(
                TEXT(
                    "[対象] %s\n"
                    "状態: %s\n"
                    "Translation  元: %.2f / %.2f / %.2f  →  変更後: %.2f / %.2f / %.2f\n"
                    "Rotation     元: %.2f / %.2f / %.2f  →  変更後: %.2f / %.2f / %.2f\n"
                    "Uniform Scale 元: %.2f  →  変更後: %.2f\n\n"
                ),
                *Item->AssetName,
                *Item->Status,
                Item->OriginalTranslation.X, Item->OriginalTranslation.Y, Item->OriginalTranslation.Z,
                NewTranslation.X, NewTranslation.Y, NewTranslation.Z,
                Item->OriginalRotation.Roll, Item->OriginalRotation.Pitch, Item->OriginalRotation.Yaw,
                NewRotation.Roll, NewRotation.Pitch, NewRotation.Yaw,
                Item->OriginalUniformScale, NewScale
            );
        }

        if (Result.IsEmpty())
        {
            Result = TEXT("コンテンツブラウザでStatic Meshを選択してください。");
        }

        return Result;
    }

    FString BuildBatchLog(const TSharedPtr<FFBXBatchState>& State)
    {
        return State.IsValid() && !State->LogMessages.IsEmpty()
            ? FString::Join(State->LogMessages, TEXT("\n"))
            : TEXT("まだログはありません。");
    }
}

void SFBXBatchReimportWindow::Construct(const FArguments& Arguments)
{
    BatchState = MakeShared<FFBXBatchState>();
    BatchState->LogMessages.Add(FString::Printf(
        TEXT("[%s] バッチウィンドウを初期化しました。"),
        *FDateTime::Now().ToString(TEXT("%H:%M:%S"))));

    FContentBrowserModule& ContentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

    TArray<FAssetData> SelectedAssets;
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

    for (const FAssetData& AssetData : SelectedAssets)
    {
        TSharedPtr<FFBXBatchItem> Item = MakeShared<FFBXBatchItem>();
        UObject* Asset = AssetData.GetAsset();

        if (!IsValid(Asset))
        {
            Item->AssetName = AssetData.AssetName.ToString();
            Item->Status = TEXT("対象外: アセットをロードできません。");
            Item->bEligible = false;
            BatchState->Items.Add(Item);
            continue;
        }

        UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);
        if (!IsValid(StaticMesh))
        {
            Item->AssetName = Asset->GetName();
            Item->Status = TEXT("対象外: Static Meshではありません。");
            Item->bEligible = false;
            BatchState->Items.Add(Item);
            continue;
        }

        Item->AssetName = StaticMesh->GetName();

        FString ValidationError;
        TSharedPtr<FFBXImportSettingsDraft> Draft;
        if (!FFBXReimportService::TryCreateDraft(StaticMesh, Draft, ValidationError) || !Draft.IsValid())
        {
            Item->Status = FString::Printf(TEXT("対象外: %s"), *ValidationError);
            Item->bEligible = false;
            BatchState->Items.Add(Item);
            continue;
        }

        Item->Draft = Draft;
        Item->SourceFilePath = Draft->SourceFilePath;
        Item->Status = TEXT("待機中");
        Item->bEligible = true;
        Item->OriginalTranslation = Draft->Translation;
        Item->OriginalRotation = Draft->Rotation;
        Item->OriginalUniformScale = Draft->UniformScale;
        BatchState->Items.Add(Item);
    }

    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("← ホームに戻る")))
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
        .OnClicked_Lambda([OnBackToHome = Arguments._OnBackToHome]()
        {
            OnBackToHome.ExecuteIfBound();
            return FReply::Handled();
        })
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("FBX Batch Reimport Assistant")))
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]()
        {
            int32 EligibleCount = 0;
            int32 ExcludedCount = 0;
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
            {
                if (Item.IsValid() && Item->bEligible) ++EligibleCount;
                else ++ExcludedCount;
            }
            return FText::FromString(FString::Printf(
                TEXT("選択アセット数: %d\n一括変更・再インポート対象: %d\n対象外: %d"),
                State->Items.Num(), EligibleCount, ExcludedCount));
        })
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("一括設定（チェックした項目だけ入力できます）")))
    ];

    // Uniform Scale
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SCheckBox)
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
        .IsChecked_Lambda([State = BatchState]() { return State->OverrideSettings.bOverrideUniformScale ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([State = BatchState](ECheckBoxState NewState)
        {
            State->OverrideSettings.bOverrideUniformScale = NewState == ECheckBoxState::Checked;
            MarkBatchOverridesEdited(State);
        })
        [ SNew(STextBlock).Text(FText::FromString(TEXT("Uniform Scaleを全対象に同じ値で上書き"))) ]
    ];
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("Uniform Scale")), 1.0f,
            [State = BatchState](float V) { State->OverrideSettings.UniformScale = V; MarkBatchOverridesEdited(State); },
            [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideUniformScale; })
    ];

    // Translation
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SCheckBox)
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
        .IsChecked_Lambda([State = BatchState]() { return State->OverrideSettings.bOverrideTranslation ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([State = BatchState](ECheckBoxState NewState)
        {
            State->OverrideSettings.bOverrideTranslation = NewState == ECheckBoxState::Checked;
            MarkBatchOverridesEdited(State);
        })
        [ SNew(STextBlock).Text(FText::FromString(TEXT("Translation Offsetを全対象に同じ値で上書き"))) ]
    ];
    TSharedRef<SHorizontalBox> TranslationRow = SNew(SHorizontalBox);
    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ MakeBatchNumericField(FText::FromString(TEXT("X")), 0.0f,
        [State = BatchState](float V) { State->OverrideSettings.Translation.X = V; MarkBatchOverridesEdited(State); },
        [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideTranslation; }) ];
    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ MakeBatchNumericField(FText::FromString(TEXT("Y")), 0.0f,
        [State = BatchState](float V) { State->OverrideSettings.Translation.Y = V; MarkBatchOverridesEdited(State); },
        [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideTranslation; }) ];
    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ MakeBatchNumericField(FText::FromString(TEXT("Z")), 0.0f,
        [State = BatchState](float V) { State->OverrideSettings.Translation.Z = V; MarkBatchOverridesEdited(State); },
        [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideTranslation; }) ];
    Root->AddSlot().AutoHeight().Padding(4.0f)[TranslationRow];

    // Rotation
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SCheckBox)
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
        .IsChecked_Lambda([State = BatchState]() { return State->OverrideSettings.bOverrideRotation ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([State = BatchState](ECheckBoxState NewState)
        {
            State->OverrideSettings.bOverrideRotation = NewState == ECheckBoxState::Checked;
            MarkBatchOverridesEdited(State);
        })
        [ SNew(STextBlock).Text(FText::FromString(TEXT("Rotation Offsetを全対象に同じ値で上書き"))) ]
    ];
    TSharedRef<SHorizontalBox> RotationRow = SNew(SHorizontalBox);
    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ MakeBatchNumericField(FText::FromString(TEXT("Roll")), 0.0f,
        [State = BatchState](float V) { State->OverrideSettings.Rotation.Roll = V; MarkBatchOverridesEdited(State); },
        [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideRotation; }) ];
    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ MakeBatchNumericField(FText::FromString(TEXT("Pitch")), 0.0f,
        [State = BatchState](float V) { State->OverrideSettings.Rotation.Pitch = V; MarkBatchOverridesEdited(State); },
        [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideRotation; }) ];
    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ MakeBatchNumericField(FText::FromString(TEXT("Yaw")), 0.0f,
        [State = BatchState](float V) { State->OverrideSettings.Rotation.Yaw = V; MarkBatchOverridesEdited(State); },
        [State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->OverrideSettings.bOverrideRotation; }) ];
    Root->AddSlot().AutoHeight().Padding(4.0f)[RotationRow];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [ SNew(STextBlock).Text(FText::FromString(TEXT("変更前 → 変更後のプレビュー"))) ];
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]() { return FText::FromString(BuildBatchPreview(State)); })
        .AutoWrapText(true)
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("一括設定を適用")))
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->bOverridesPending; })
        .OnClicked_Lambda([State = BatchState]()
        {
            FString Message;
            FFBXReimportService::ApplyBatchOverrides(State, Message);
            State->StatusText = Message;
            State->LogMessages.Add(FString::Printf(TEXT("[%s] %s"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Message));
            return FReply::Handled();
        })
    ];

    // Batch operation controls
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("一括再インポートの進行状況")))
    ];
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SProgressBar)
        .Percent_Lambda([State = BatchState]()
        {
            if (!State.IsValid() || State->PendingCount <= 0) return 0.0f;
            return FMath::Clamp(static_cast<float>(State->ProcessedCount) / static_cast<float>(State->PendingCount), 0.0f, 1.0f);
        })
    ];
    Root->AddSlot().AutoHeight().Padding(4.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]()
        {
            return FText::FromString(FString::Printf(
                TEXT("処理済み: %d / %d    成功: %d    失敗: %d    スキップ: %d"),
                State->ProcessedCount, State->PendingCount, State->SuccessCount, State->FailedCount, State->SkippedCount));
        })
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("対象のFBXを一括再インポート")))
        .IsEnabled_Lambda([State = BatchState]()
        {
            if (!State.IsValid() || State->bIsRunning || State->bOverridesPending) return false;
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
                if (Item.IsValid() && Item->bEligible && Item->Draft.IsValid()) return true;
            return false;
        })
        .OnClicked_Lambda([State = BatchState]()
        {
            int32 EligibleCount = 0;
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
                if (Item.IsValid() && Item->bEligible && Item->Draft.IsValid()) ++EligibleCount;

            if (EligibleCount == 0)
            {
                State->StatusText = TEXT("再インポート可能なアセットがありません。");
                return FReply::Handled();
            }

            const EAppReturnType::Type Response = FMessageDialog::Open(
                EAppMsgType::YesNo,
                FText::FromString(FString::Printf(
                    TEXT("再インポート対象: %d\n選択アセット数: %d\n\n変更した設定で順番に再インポートします。実行しますか？"),
                    EligibleCount, State->Items.Num())));
            if (Response == EAppReturnType::Yes) FFBXReimportService::StartBatch(State, false);
            else State->StatusText = TEXT("一括再インポートをキャンセルしました。");
            return FReply::Handled();
        })
    ];

    Root->AddSlot().AutoHeight().Padding(6.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("現在のアセット完了後に中断")))
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && State->bIsRunning && !State->bCancelRequested; })
        .OnClicked_Lambda([State = BatchState]()
        {
            FFBXReimportService::RequestCancelBatch(State);
            return FReply::Handled();
        })
    ];

    Root->AddSlot().AutoHeight().Padding(6.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("失敗したアセットだけ再試行")))
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning && !State->bOverridesPending && State->FailedCount > 0; })
        .OnClicked_Lambda([State = BatchState]()
        {
            FFBXReimportService::StartBatch(State, true);
            return FReply::Handled();
        })
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]() { return FText::FromString(State->StatusText); })
        .AutoWrapText(true)
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [ SNew(STextBlock).Text(FText::FromString(TEXT("処理ログ"))) ];
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]() { return FText::FromString(BuildBatchLog(State)); })
        .AutoWrapText(true)
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT(
            "使い方:\n"
            "1. 一括変更したい設定だけチェックします。未チェックの入力欄はグレーアウトし、編集できません。\n"
            "2. 共通値を入力して変更前後を確認します。\n"
            "3. 一括設定を適用し、アセットを保存してから再インポートします。\n"
            "4. 中断を押すと、現在処理中のアセットが完了した後に次のアセットへ進まず停止します。\n"
            "5. 失敗したアセットは、失敗理由を確認した後に個別再試行できます。\n\n"
            "チェックを外した項目は各メッシュの元の値を維持します。対象外アセットは処理しません。"
        )))
        .AutoWrapText(true)
    ];

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()[Root]
    ];
}
