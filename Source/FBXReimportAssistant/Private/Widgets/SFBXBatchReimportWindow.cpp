#include "Widgets/SFBXBatchReimportWindow.h"

#include "Services/FBXReimportService.h"
#include "Styles/FBXReimportAssistantStyle.h"
#include "AssetThumbnail.h"
#include "Styling/SlateTypes.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SFBXPresetPanel.h"
#include "Misc/Paths.h"

namespace
{
    TSharedRef<SVerticalBox> MakeBatchNumericField
    (
        const FText& Label,
        TFunction<float()> GetValue,
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
                    .Font(FFBXReimportAssistantStyle::Get().GetFontStyle(TEXT("Font.Caption")))
                    .ColorAndOpacity(FFBXReimportAssistantStyle::Get().GetSlateColor(TEXT("Color.TextSecondary")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2.0f)
                [
                    SNew(SSpinBox<float>)
                    .Value_Lambda([GetValue]()
                    {
                        return GetValue();
                    })
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

            if (!Item->bSelectedForBatch)
            {
                Result += FString::Printf(
                    TEXT("[選択解除] %s\n一括設定・再インポートの対象外（このアセットの設定は変更しません）\n\n"),
                    *Item->AssetName
                );
                continue;
            }

            const FFBXBatchOverrideSettings& Overrides = State->OverrideSettings;
            const FVector NewTranslation = Overrides.bOverrideTranslation
                ? Overrides.Translation : Item->OriginalTranslation;
            const FRotator NewRotation = Overrides.bOverrideRotation
                ? Overrides.Rotation : Item->OriginalRotation;
            const float NewScale = Overrides.bOverrideUniformScale
                ? Overrides.UniformScale : Item->OriginalUniformScale;

            Result += FString::Printf(
                TEXT(
                    "[%s] %s\n"
                    "Translation  %.2f / %.2f / %.2f  →  %.2f / %.2f / %.2f\n"
                    "Rotation     %.2f / %.2f / %.2f  →  %.2f / %.2f / %.2f\n"
                    "Uniform Scale %.2f  →  %.2f\n\n"
                ),
                TEXT("処理対象"),
                *Item->AssetName,
                Item->Draft->Translation.X, Item->Draft->Translation.Y, Item->Draft->Translation.Z,
                NewTranslation.X, NewTranslation.Y, NewTranslation.Z,
                Item->Draft->Rotation.Roll, Item->Draft->Rotation.Pitch, Item->Draft->Rotation.Yaw,
                NewRotation.Roll, NewRotation.Pitch, NewRotation.Yaw,
                Item->Draft->UniformScale, NewScale
            );
        }

        if (Result.IsEmpty())
        {
            Result = TEXT("コンテンツブラウザでStatic MeshまたはSkeletal Meshを選択してください。");
        }

        return Result;
    }

    FString BuildBatchLog(const TSharedPtr<FFBXBatchState>& State)
    {
        return State.IsValid() && !State->LogMessages.IsEmpty()
            ? FString::Join(State->LogMessages, TEXT("\n"))
            : TEXT("まだログはありません。");
    }


void SFBXBatchReimportWindow::RefreshFilteredAssetList()
{
    FilteredItems.Reset();

    if (BatchState.IsValid())
    {
        const FString NormalizedSearch = SearchText.TrimStartAndEnd();

        for (const TSharedPtr<FFBXBatchItem>& Item : BatchState->Items)
        {
            if (!Item.IsValid())
            {
                continue;
            }

            const bool bMatchesSearch = NormalizedSearch.IsEmpty() ||
                Item->AssetName.Contains(NormalizedSearch, ESearchCase::IgnoreCase) ||
                Item->AssetObjectPath.Contains(NormalizedSearch, ESearchCase::IgnoreCase) ||
                Item->SourceFilePath.Contains(NormalizedSearch, ESearchCase::IgnoreCase) ||
                Item->Status.Contains(NormalizedSearch, ESearchCase::IgnoreCase);

            if (bMatchesSearch)
            {
                FilteredItems.Add(Item);
            }
        }
    }

    if (AssetListView.IsValid())
    {
        AssetListView->RequestListRefresh();
    }
}

void SFBXBatchReimportWindow::SetAllAssetSelection(bool bSelected)
{
    if (!BatchState.IsValid() || BatchState->bIsRunning)
    {
        return;
    }

    // 検索で現在表示されている行だけを一括選択・解除する。
    bool bSelectionChanged = false;
    for (const TSharedPtr<FFBXBatchItem>& Item : FilteredItems)
    {
        if (Item.IsValid() && Item->bEligible && Item->bSelectedForBatch != bSelected)
        {
            Item->bSelectedForBatch = bSelected;
            bSelectionChanged = true;
        }
    }

    // 選択状態はバッチ設定の適用対象も変えるため、未適用状態として扱う。
    if (bSelectionChanged && (BatchState->bBatchOverridesEverApplied || BatchState->OverrideSettings.HasAnyOverride()))
    {
        BatchState->bOverridesPending = true;
        BatchState->StatusText = TEXT("処理対象の選択が変更されました。必要に応じて「一括設定を適用」してください。");
    }

    if (AssetListView.IsValid())
    {
        AssetListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> SFBXBatchReimportWindow::OnGenerateAssetRow(
    TSharedPtr<FFBXBatchItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable
)
{
    const ISlateStyle& Style = FFBXReimportAssistantStyle::Get();

    if (!Item.IsValid())
    {
        return SNew(STableRow<TSharedPtr<FFBXBatchItem>>, OwnerTable)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("無効なアセット")))
        ];
    }

    TSharedPtr<FAssetThumbnail>& Thumbnail = ThumbnailByItem.FindOrAdd(Item.Get());
    if (!Thumbnail.IsValid() && ThumbnailPool.IsValid() && Item->AssetData.IsValid())
    {
        Thumbnail = MakeShared<FAssetThumbnail>(Item->AssetData, 72, 72, ThumbnailPool);
    }

    TSharedRef<SWidget> ThumbnailContent =
        SNew(STextBlock).Text(FText::FromString(TEXT("No Preview")));

    if (Thumbnail.IsValid())
    {
        FAssetThumbnailConfig ThumbnailConfig;
        ThumbnailContent = Thumbnail->MakeThumbnailWidget(ThumbnailConfig);
    }

    TSharedRef<SWidget> ThumbnailWidget = SNew(SBox)
        .WidthOverride(72.0f)
        .HeightOverride(72.0f)
        [ ThumbnailContent ];

    return SNew(STableRow<TSharedPtr<FFBXBatchItem>>, OwnerTable)
        .Padding(FMargin(4.0f, 3.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f)
            [
                SNew(SCheckBox)
                .IsEnabled_Lambda([State = BatchState, Item]()
                {
                    return State.IsValid() && !State->bIsRunning && Item.IsValid() && Item->bEligible;
                })
                .IsChecked_Lambda([Item]()
                {
                    return Item.IsValid() && Item->bSelectedForBatch
                        ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([State = BatchState, Item](ECheckBoxState NewState)
                {
                    if (!State.IsValid() || State->bIsRunning || !Item.IsValid() || !Item->bEligible)
                    {
                        return;
                    }

                    Item->bSelectedForBatch = NewState == ECheckBoxState::Checked;
                    if (State->bBatchOverridesEverApplied || State->OverrideSettings.HasAnyOverride())
                    {
                        State->bOverridesPending = true;
                        State->StatusText = TEXT("処理対象の選択が変更されました。設定を適用してから再インポートしてください。");
                    }
                })
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f)
            [ ThumbnailWidget ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 2.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(
                        Item->Draft.IsValid() && !Item->Draft->MeshTypeLabel.IsEmpty()
                            ? FString::Printf(TEXT("%s  [%s]"), *Item->AssetName, *Item->Draft->MeshTypeLabel)
                            : Item->AssetName))
                    .Font(Style.GetFontStyle(TEXT("Font.Body")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item->AssetObjectPath))
                    .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                    .AutoWrapText(true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([Item]()
                    {
                        if (!Item.IsValid()) return FText::GetEmpty();
                        return FText::FromString(Item->Status);
                    })
                    .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                    .ColorAndOpacity_Lambda([Item]()
                    {
                        if (!Item.IsValid() || !Item->bEligible || Item->Status.StartsWith(TEXT("失敗")))
                        {
                            return FSlateColor(FLinearColor(0.78f, 0.42f, 0.36f, 1.0f));
                        }
                        if (Item->Status.StartsWith(TEXT("完了")))
                        {
                            return FSlateColor(FLinearColor(0.35f, 0.68f, 0.48f, 1.0f));
                        }
                        if (Item->Status.Contains(TEXT("再インポート中")))
                        {
                            return FSlateColor(FLinearColor(0.78f, 0.63f, 0.34f, 1.0f));
                        }
                        return FSlateColor::UseSubduedForeground();
                    })
                    .AutoWrapText(true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item->SourceFilePath.IsEmpty()
                        ? TEXT("FBXソース：なし")
                        : FString::Printf(TEXT("FBX：%s"), *Item->SourceFilePath)))
                    .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                    .AutoWrapText(true)
                ]
            ]
        ];
}

void SFBXBatchReimportWindow::Construct(const FArguments& Arguments)
{
    BatchState = MakeShared<FFBXBatchState>();
    ThumbnailPool = MakeShared<FAssetThumbnailPool>(128, 0.01, 2);
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
        Item->AssetData = AssetData;
        Item->AssetName = AssetData.AssetName.ToString();
        Item->AssetObjectPath = AssetData.ObjectPath.ToString();
        UObject* Asset = AssetData.GetAsset();

        if (!IsValid(Asset))
        {
            Item->AssetName = AssetData.AssetName.ToString();
            Item->Status = TEXT("対象外: アセットをロードできません。");
            Item->bEligible = false;
            BatchState->Items.Add(Item);
            continue;
        }

        UObject* StaticMesh = Asset;
        if (!FFBXReimportService::IsSupportedMesh(StaticMesh))
        {
            Item->AssetName = Asset->GetName();
            Item->Status = TEXT("対象外: Static MeshまたはSkeletal Meshではありません。");
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
        Item->bSelectedForBatch = true;
        Item->OriginalTranslation = Draft->Translation;
        Item->OriginalRotation = Draft->Rotation;
        Item->OriginalUniformScale = Draft->UniformScale;
        BatchState->Items.Add(Item);
    }

    const ISlateStyle& Style = FFBXReimportAssistantStyle::Get();
    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SButton)
        .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Secondary")))
        .Text(FText::FromString(TEXT("← ホームに戻る")))
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
        .OnClicked_Lambda([OnBackToHome = Arguments._OnBackToHome]()
        {
            OnBackToHome.ExecuteIfBound();
            return FReply::Handled();
        })
    ];
    
    Root->AddSlot().AutoHeight()
    .Padding(0, 0, 0, 10)
    [
        SNew(SFBXPresetPanel)
        // 保存: 現在の一括設定（チェック状態と値）をそのままプリセットにする。
        .OnGetSettingsToSave_Lambda(
            [State = BatchState](FFBXBatchOverrideSettings& OutSettings, FString& OutError)
            {
                if (!State.IsValid())
                {
                    OutError = TEXT("設定データを取得できません。");
                    return false;
                }

                OutSettings = State->OverrideSettings;
                return true;
            }
        )
        // 読み込み: プリセットの内容を一括設定へ反映する。
        .OnPresetLoaded_Lambda(
            [State = BatchState](const FFBXImportPreset& Preset, FString& OutMessage)
            {
                if (!State.IsValid())
                {
                    OutMessage = TEXT("バッチ状態を取得できません。");
                    return false;
                }

                if (State->bIsRunning)
                {
                    OutMessage = TEXT("一括再インポート中はプリセットを読み込めません。");
                    return false;
                }

                State->OverrideSettings = Preset.Settings;
                MarkBatchOverridesEdited(State);

                OutMessage = FString::Printf(TEXT("読み込みました：%s"), *Preset.Name);
                return true;
            }
        )
    ];
    
    
    Root->AddSlot().AutoHeight().Padding(8.0f, 2.0f, 8.0f, 12.0f)
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Hero")))
        .Padding(FMargin(16.0f, 13.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("BATCH ASSET WORKFLOW")))
                .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.Success"))))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("複数アセットの一括設定・再インポート")))
                .Font(Style.GetFontStyle(TEXT("Font.Title")))
                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
            ]
        ]
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([State = BatchState]()
        {
            int32 EligibleCount = 0;
            int32 SelectedCount = 0;
            int32 IneligibleCount = 0;
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
            {
                if (!Item.IsValid()) continue;
                if (Item->bEligible)
                {
                    ++EligibleCount;
                    if (Item->bSelectedForBatch) ++SelectedCount;
                }
                else
                {
                    ++IneligibleCount;
                }
            }
            return FText::FromString(FString::Printf(
                TEXT("選択中: %d / %d    対象外: %d"),
                SelectedCount, EligibleCount, IneligibleCount));
        })
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("処理対象アセット")))
        .Font(Style.GetFontStyle(TEXT("Font.Section")))
        .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SNew(SSearchBox)
            .HintText(FText::FromString(TEXT("アセット名・パス・状態で検索...")))
            .OnTextChanged_Lambda([this](const FText& NewText)
            {
                SearchText = NewText.ToString();
                RefreshFilteredAssetList();
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("表示中をすべて選択")))
            .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
            .OnClicked_Lambda([this]()
            {
                SetAllAssetSelection(true);
                return FReply::Handled();
            })
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("表示中をすべて解除")))
            .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning; })
            .OnClicked_Lambda([this]()
            {
                SetAllAssetSelection(false);
                return FReply::Handled();
            })
        ]
    ];

    RefreshFilteredAssetList();

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(6.0f)
        [
            SNew(SBox)
            .HeightOverride(300.0f)
            [
                SAssignNew(AssetListView, SListView<TSharedPtr<FFBXBatchItem>>)
                .ListItemsSource(&FilteredItems)
                .SelectionMode(ESelectionMode::None)
                .OnGenerateRow_Lambda([this](TSharedPtr<FFBXBatchItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
                {
                    return OnGenerateAssetRow(Item, OwnerTable);
                })
            ]
        ]
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("一括設定（チェックした項目だけ入力できます）")))
        .Font(Style.GetFontStyle(TEXT("Font.Section")))
        .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
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
        MakeBatchNumericField(
            FText::FromString(TEXT("Uniform Scale")),
            // 現在の値を取得
            [State = BatchState]()
            {
                return State.IsValid()
                    ? State->OverrideSettings.UniformScale
                    : 1.0f;
            },

            // 値が変更されたとき
            [State = BatchState](float V)
            {
                State->OverrideSettings.UniformScale = V;
                MarkBatchOverridesEdited(State);
            },

            // 入力可能かどうか
            [State = BatchState]()
            {
                return State.IsValid()
                    && !State->bIsRunning
                    && State->OverrideSettings.bOverrideUniformScale;
            }
        )    
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
    [ 
        MakeBatchNumericField(
            FText::FromString(TEXT("X")),
        
            [State = BatchState]()
            {
                return State.IsValid()
                ? State->OverrideSettings.Translation.X
                : 0.0f;
            },
            
            [State = BatchState](float V)
            {
                State->OverrideSettings.Translation.X = V;
                MarkBatchOverridesEdited(State);
            },
            
            [State = BatchState]()
            {
                return State.IsValid()
                && !State->bIsRunning
                && State->OverrideSettings.bOverrideTranslation;
            }
        )    
    ];        
    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ 
        MakeBatchNumericField(
            FText::FromString(TEXT("Y")),

            [State = BatchState]()
            {
                return State.IsValid()
                ? State->OverrideSettings.Translation.Y
                : 0.0f;
            },

            [State = BatchState](float V)
            {
                State->OverrideSettings.Translation.Y = V;
                MarkBatchOverridesEdited(State);
            },

            [State = BatchState]()
            {
                return State.IsValid()
                    && !State->bIsRunning
                    && State->OverrideSettings.bOverrideTranslation;
            }
        )
    ];
    
    TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ 
        MakeBatchNumericField(
            FText::FromString(TEXT("Z")),
                
            [State = BatchState]()
            {
                return State.IsValid()
                    ? State->OverrideSettings.Translation.Z
                    : 0.0f;
            },
            
            [State = BatchState](float V)
            {
                State->OverrideSettings.Translation.Z = V;
                MarkBatchOverridesEdited(State);
            },
            
            [State = BatchState]()
            {
                return State.IsValid()
                && !State->bIsRunning
                && State->OverrideSettings.bOverrideTranslation;
            }
        )        
    ];
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
    [ 
        MakeBatchNumericField(
            FText::FromString(TEXT("Roll")),
            
            [State = BatchState]()
            {
                return State.IsValid()
                    ? State->OverrideSettings.Rotation.Roll
                    : 0.0f;
            },
            
            [State = BatchState](float V)
            {
                State->OverrideSettings.Rotation.Roll = V;
                MarkBatchOverridesEdited(State);
            },
            
            [State = BatchState]()
            {
                return State.IsValid()
                    && !State->bIsRunning
                    && State->OverrideSettings.bOverrideRotation;
            }
        )    
    ];
    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [ 
        MakeBatchNumericField(
            FText::FromString(TEXT("Pitch")),
            
            [State = BatchState]()
            {
                return State.IsValid()
                    ? State->OverrideSettings.Rotation.Pitch
                    : 0.0f;
            },
            
            [State = BatchState](float V)
            {
                State->OverrideSettings.Rotation.Pitch = V;
                MarkBatchOverridesEdited(State);
            },
            
            [State = BatchState]()
            {
                return State.IsValid()
                    && !State->bIsRunning
                    && State->OverrideSettings.bOverrideRotation;
            }
        )    
    ];
    RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
    [
        MakeBatchNumericField(
            FText::FromString(TEXT("Yaw")),
            
            [State = BatchState]()
            {
                return State.IsValid()
                    ? State->OverrideSettings.Rotation.Yaw
                    : 0.0f;
            },
            
            [State = BatchState](float V)
            {
                State->OverrideSettings.Rotation.Yaw = V;
                MarkBatchOverridesEdited(State);
            },
            
            [State = BatchState]()
            {
                return State.IsValid()
                    && !State->bIsRunning
                    && State->OverrideSettings.bOverrideRotation;
            }
        )    
    ];
    Root->AddSlot().AutoHeight().Padding(4.0f)[RotationRow];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [ SNew(STextBlock)
        .Text(FText::FromString(TEXT("変更前 → 変更後のプレビュー")))
        .Font(Style.GetFontStyle(TEXT("Font.Section")))
        .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover")))) ];
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Surface")))
        .Padding(FMargin(12.0f))
        [
            SNew(STextBlock)
            .Text_Lambda([State = BatchState]() { return FText::FromString(BuildBatchPreview(State)); })
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
        ]
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SButton)
        .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Primary")))
        .Text(FText::FromString(TEXT("一括設定を適用")))
        .IsEnabled_Lambda([State = BatchState]() { return State.IsValid() && !State->bIsRunning && State->bOverridesPending; })
        .OnClicked_Lambda([State = BatchState]()
        {
            // 選択中にSkeletal Meshが含まれる場合は、適用前に確認する。
            int32 SkeletalCount = 0;
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
            {
                if (Item.IsValid() && Item->bEligible && Item->bSelectedForBatch &&
                    Item->Draft.IsValid() && Item->Draft->bIsSkeletalMesh)
                {
                    ++SkeletalCount;
                }
            }

            if (SkeletalCount > 0)
            {
                const EAppReturnType::Type Response = FMessageDialog::Open(
                    EAppMsgType::YesNo,
                    FText::FromString(FString::Printf(
                        TEXT("選択中にSkeletal Meshが%d件含まれています。\n"
                             "オフセットを変更すると、そのスケルトンを使用するアニメーションの見え方がずれる場合があります。\n\n"
                             "一括設定を適用しますか？"),
                        SkeletalCount)));

                if (Response != EAppReturnType::Yes)
                {
                    State->StatusText = TEXT("一括設定の適用をキャンセルしました。");
                    return FReply::Handled();
                }
            }

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
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("一括再インポートの進行状況")))
        .Font(Style.GetFontStyle(TEXT("Font.Section")))
        .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
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
        .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Success")))
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
                if (Item.IsValid() && Item->bEligible && Item->bSelectedForBatch && Item->Draft.IsValid()) ++EligibleCount;

            if (EligibleCount == 0)
            {
                State->StatusText = TEXT("再インポート可能なアセットがありません。");
                return FReply::Handled();
            }

            const EAppReturnType::Type Response = FMessageDialog::Open(
                EAppMsgType::YesNo,
                FText::FromString(FString::Printf(
                    TEXT("再インポート対象: %d\n一覧内のアセット数: %d\n\nチェックしたアセットだけを順番に再インポートします。実行しますか？"),
                    EligibleCount, State->Items.Num())));
            if (Response == EAppReturnType::Yes) FFBXReimportService::StartBatch(State, false);
            else State->StatusText = TEXT("一括再インポートをキャンセルしました。");
            return FReply::Handled();
        })
    ];

    Root->AddSlot().AutoHeight().Padding(6.0f)
    [
        SNew(SButton)
        .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Secondary")))
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
        .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Secondary")))
        .Text(FText::FromString(TEXT("選択中の失敗アセットだけ再試行")))
        .IsEnabled_Lambda([State = BatchState]()
        {
            if (!State.IsValid() || State->bIsRunning || State->bOverridesPending)
            {
                return false;
            }
            for (const TSharedPtr<FFBXBatchItem>& Item : State->Items)
            {
                if (Item.IsValid() && Item->bEligible && Item->bSelectedForBatch && Item->Status.StartsWith(TEXT("失敗")))
                {
                    return true;
                }
            }
            return false;
        })
        .OnClicked_Lambda([State = BatchState]()
        {
            FFBXReimportService::StartBatch(State, true);
            return FReply::Handled();
        })
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Surface")))
        .Padding(FMargin(12.0f))
        [
            SNew(STextBlock)
            .Text_Lambda([State = BatchState]() { return FText::FromString(State->StatusText); })
            .Font(Style.GetFontStyle(TEXT("Font.Body")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
            .AutoWrapText(true)
        ]
    ];

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [ SNew(STextBlock)
        .Text(FText::FromString(TEXT("処理ログ")))
        .Font(Style.GetFontStyle(TEXT("Font.Section")))
        .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover")))) ];
    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Surface")))
        .Padding(FMargin(12.0f))
        [
            SNew(STextBlock)
            .Text_Lambda([State = BatchState]() { return FText::FromString(BuildBatchLog(State)); })
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
        ]
    ];

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Surface")))
        .Padding(FMargin(12.0f))
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
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
        ]
    ];

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Background")))
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()[Root]
        ]
    ];
}
