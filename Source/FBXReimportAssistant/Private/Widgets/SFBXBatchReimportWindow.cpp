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

namespace
{
    TSharedRef<SVerticalBox> MakeBatchNumericField(
        const FText& Label,
        float InitialValue,
        TFunction<void(float)> OnValueChanged
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

        // 以前に一括適用した値がある状態で全チェックを外した場合も、
        // 元の個別値へ戻すために「適用待ち」にする。
        State->bOverridesPending =
            State->OverrideSettings.HasAnyOverride() ||
            State->bBatchOverridesEverApplied;

        if (State->bOverridesPending)
        {
            State->StatusText = TEXT("一括設定に未適用の変更があります。");
        }
        else
        {
            State->StatusText = TEXT("一括変更は選択されていません。各アセットの元の設定を使用します。");
        }
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

            const FVector NewTranslation = Overrides.bOverrideTranslation
                ? Overrides.Translation
                : Item->OriginalTranslation;

            const FRotator NewRotation = Overrides.bOverrideRotation
                ? Overrides.Rotation
                : Item->OriginalRotation;

            const float NewScale = Overrides.bOverrideUniformScale
                ? Overrides.UniformScale
                : Item->OriginalUniformScale;

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
                Item->OriginalTranslation.X,
                Item->OriginalTranslation.Y,
                Item->OriginalTranslation.Z,
                NewTranslation.X,
                NewTranslation.Y,
                NewTranslation.Z,
                Item->OriginalRotation.Roll,
                Item->OriginalRotation.Pitch,
                Item->OriginalRotation.Yaw,
                NewRotation.Roll,
                NewRotation.Pitch,
                NewRotation.Yaw,
                Item->OriginalUniformScale,
                NewScale
            );
        }

        if (Result.IsEmpty())
        {
            Result = TEXT("コンテンツブラウザでStatic Meshを選択してください。");
        }

        return Result;
    }
}

void SFBXBatchReimportWindow::Construct(const FArguments& Arguments)
{
    BatchState = MakeShared<FFBXBatchState>();

    // =====================================
    // ① Content Browserの選択アセットを取得
    // =====================================

    FContentBrowserModule& ContentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

    TArray<FAssetData> SelectedAssets;
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

    // =====================================
    // ② 各アセットを検証して一覧へ登録
    // =====================================

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

        // 一括変更前の元の値を保存する。
        Item->OriginalTranslation = Draft->Translation;
        Item->OriginalRotation = Draft->Rotation;
        Item->OriginalUniformScale = Draft->UniformScale;

        BatchState->Items.Add(Item);
    }

    // =====================================
    // ③ UIルート
    // =====================================

    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);

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
                if (Item.IsValid() && Item->bEligible)
                {
                    ++EligibleCount;
                }
                else
                {
                    ++ExcludedCount;
                }
            }

            return FText::FromString(FString::Printf(
                TEXT("選択アセット数: %d\n一括変更・再インポート対象: %d\n対象外: %d"),
                State->Items.Num(), EligibleCount, ExcludedCount));
        })
    ];

    // =====================================
    // ④ Uniform Scaleの一括設定
    // =====================================

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SCheckBox)
        .IsChecked_Lambda([State = BatchState]()
        {
            return State->OverrideSettings.bOverrideUniformScale
                ? ECheckBoxState::Checked
                : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([State = BatchState](ECheckBoxState NewState)
        {
            State->OverrideSettings.bOverrideUniformScale =
                NewState == ECheckBoxState::Checked;
            MarkBatchOverridesEdited(State);
        })
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Uniform Scaleを全対象に同じ値で上書き")))
        ]
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        MakeBatchNumericField(
            FText::FromString(TEXT("Uniform Scale")),
            BatchState->OverrideSettings.UniformScale,
            [State = BatchState](float NewValue)
            {
                State->OverrideSettings.UniformScale = NewValue;
                MarkBatchOverridesEdited(State);
            }
        )
    ];

    // =====================================
    // ⑤ Translationの一括設定
    // =====================================

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SCheckBox)
        .IsChecked_Lambda([State = BatchState]()
        {
            return State->OverrideSettings.bOverrideTranslation
                ? ECheckBoxState::Checked
                : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([State = BatchState](ECheckBoxState NewState)
        {
            State->OverrideSettings.bOverrideTranslation =
                NewState == ECheckBoxState::Checked;
            MarkBatchOverridesEdited(State);
        })
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Translation Offsetを全対象に同じ値で上書き")))
        ]
    ];

    TSharedRef<SHorizontalBox> TranslationRow = SNew(SHorizontalBox);

    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("X")), 0.0f,
            [State = BatchState](float Value)
            {
                State->OverrideSettings.Translation.X = Value;
                MarkBatchOverridesEdited(State);
            })
    ];

    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("Y")), 0.0f,
            [State = BatchState](float Value)
            {
                State->OverrideSettings.Translation.Y = Value;
                MarkBatchOverridesEdited(State);
            })
    ];

    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("Z")), 0.0f,
            [State = BatchState](float Value)
            {
                State->OverrideSettings.Translation.Z = Value;
                MarkBatchOverridesEdited(State);
            })
    ];

    Root->AddSlot().AutoHeight().Padding(4.0f)[TranslationRow];

    // =====================================
    // ⑥ Rotationの一括設定
    // =====================================

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SCheckBox)
        .IsChecked_Lambda([State = BatchState]()
        {
            return State->OverrideSettings.bOverrideRotation
                ? ECheckBoxState::Checked
                : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([State = BatchState](ECheckBoxState NewState)
        {
            State->OverrideSettings.bOverrideRotation =
                NewState == ECheckBoxState::Checked;
            MarkBatchOverridesEdited(State);
        })
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Rotation Offsetを全対象に同じ値で上書き")))
        ]
    ];

    TSharedRef<SHorizontalBox> RotationRow = SNew(SHorizontalBox);

    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("Roll")), 0.0f,
            [State = BatchState](float Value)
            {
                State->OverrideSettings.Rotation.Roll = Value;
                MarkBatchOverridesEdited(State);
            })
    ];

    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("Pitch")), 0.0f,
            [State = BatchState](float Value)
            {
                State->OverrideSettings.Rotation.Pitch = Value;
                MarkBatchOverridesEdited(State);
            })
    ];

    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(FText::FromString(TEXT("Yaw")), 0.0f,
            [State = BatchState](float Value)
            {
                State->OverrideSettings.Rotation.Yaw = Value;
                MarkBatchOverridesEdited(State);
            })
    ];

    Root->AddSlot().AutoHeight().Padding(4.0f)[RotationRow];

    // =====================================
    // ⑦ 変更前後のプレビュー
    // =====================================

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("変更前 → 変更後のプレビュー")))
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]()
        {
            return FText::FromString(BuildBatchPreview(State));
        })
        .AutoWrapText(true)
    ];

    // =====================================
    // ⑧ 一括設定適用ボタン
    // =====================================

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("一括設定を適用")))
        .IsEnabled_Lambda([State = BatchState]()
        {
            return State.IsValid() &&
                !State->bIsRunning &&
                State->bOverridesPending;
        })
        .OnClicked_Lambda([State = BatchState]()
        {
            FString Message;
            FFBXReimportService::ApplyBatchOverrides(State, Message);
            State->StatusText = Message;
            return FReply::Handled();
        })
    ];

    // =====================================
    // ⑨ 一括再インポートボタン
    // =====================================

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("対象のFBXを一括再インポート")))
        .IsEnabled_Lambda([State = BatchState]()
        {
            if (!State.IsValid() || State->bIsRunning || State->bOverridesPending)
            {
                return false;
            }

            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
            {
                if (Item.IsValid() && Item->bEligible && Item->Draft.IsValid())
                {
                    return true;
                }
            }

            return false;
        })
        .OnClicked_Lambda([State = BatchState]()
        {
            int32 EligibleCount = 0;
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
            {
                if (Item.IsValid() && Item->bEligible && Item->Draft.IsValid())
                {
                    ++EligibleCount;
                }
            }

            if (EligibleCount == 0)
            {
                State->StatusText = TEXT("再インポート可能なアセットがありません。");
                return FReply::Handled();
            }

            const EAppReturnType::Type Response = FMessageDialog::Open(
                EAppMsgType::YesNo,
                FText::FromString(FString::Printf(
                    TEXT("再インポート対象: %d\n選択アセット数: %d\n\n変更した共通設定で順番に再インポートします。実行しますか？"),
                    EligibleCount,
                    State->Items.Num()
                ))
            );

            if (Response == EAppReturnType::Yes)
            {
                FFBXReimportService::StartBatch(State);
            }
            else
            {
                State->StatusText = TEXT("一括再インポートをキャンセルしました。");
            }

            return FReply::Handled();
        })
    ];

    // =====================================
    // ⑩ 処理状態
    // =====================================

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]()
        {
            return FText::FromString(State->StatusText);
        })
        .AutoWrapText(true)
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(
            TEXT(
                "使い方:\n"
                "1. 一括変更したい設定のチェックを入れます。\n"
                "2. 共通値を入力して変更前後を確認します。\n"
                "3. 一括設定を適用します。\n"
                "4. アセットを保存してから一括再インポートします。\n\n"
                "チェックを外した項目は、各メッシュの元の値に戻します。\n"
                "対象外アセットは変更・再インポートしません。"
            )
        ))
        .AutoWrapText(true)
    ];

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            Root
        ]
    ];
}
