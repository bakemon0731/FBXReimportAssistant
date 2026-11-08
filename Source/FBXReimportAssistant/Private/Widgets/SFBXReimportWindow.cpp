#include "Widgets/SFBXReimportWindow.h"

#include "Services/FBXReimportService.h"
#include "Styles/FBXReimportAssistantStyle.h"
#include "Styling/SlateTypes.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SFBXPresetPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Misc/MessageDialog.h"

namespace
{
    // 変更前 → 変更後のプレビュー文字列。変更がある項目には●を付ける。
    FString BuildDraftPreview(const FFBXImportSettingsDraft& D)
    {
        const bool bTranslationChanged = !D.Translation.Equals(D.OriginalTranslation, KINDA_SMALL_NUMBER);
        const bool bRotationChanged = !D.Rotation.Equals(D.OriginalRotation, KINDA_SMALL_NUMBER);
        const bool bScaleChanged = !FMath::IsNearlyEqual(D.UniformScale, D.OriginalUniformScale);

        const FString Header = (bTranslationChanged || bRotationChanged || bScaleChanged)
            ? TEXT("未適用の変更があります（●が変更項目）\n\n")
            : TEXT("アセットに保存されている値からの変更はありません。\n\n");

        return Header + FString::Printf(
            TEXT(
                "%sTranslation  %.2f / %.2f / %.2f  →  %.2f / %.2f / %.2f\n"
                "%sRotation     %.2f / %.2f / %.2f  →  %.2f / %.2f / %.2f\n"
                "%sUniform Scale %.2f  →  %.2f"
            ),
            bTranslationChanged ? TEXT("● ") : TEXT("   "),
            D.OriginalTranslation.X, D.OriginalTranslation.Y, D.OriginalTranslation.Z,
            D.Translation.X, D.Translation.Y, D.Translation.Z,
            bRotationChanged ? TEXT("● ") : TEXT("   "),
            D.OriginalRotation.Roll, D.OriginalRotation.Pitch, D.OriginalRotation.Yaw,
            D.Rotation.Roll, D.Rotation.Pitch, D.Rotation.Yaw,
            bScaleChanged ? TEXT("● ") : TEXT("   "),
            D.OriginalUniformScale, D.UniformScale
        );
    }
}

TSharedRef<SVerticalBox> SFBXReimportWindow::MakeNumericField(
    const FText& Label,
    TFunction<float()> GetValue,
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
            .OnValueChanged_Lambda(
                [OnValueChanged](float NewValue)
                {
                    OnValueChanged(NewValue);
                }
            )
        ];
}

void SFBXReimportWindow::Construct(const FArguments& Arguments)
{
    Draft.Reset();
    ApplyStatus = MakeShared<FString>(TEXT("まだ設定は適用されていません。"));
    ReimportStatus = MakeShared<FString>(TEXT("再インポートはまだ実行されていません。"));

    FContentBrowserModule& ContentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

    TArray<FAssetData> SelectedAssets;
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

    FString DiagnosticText;

    if (SelectedAssets.Num() != 1)
    {
        DiagnosticText = TEXT("コンテンツブラウザでStatic MeshまたはSkeletal Meshを1つだけ選択してください。");
    }
    else
    {
        UObject* Asset = SelectedAssets[0].GetAsset();
        UObject* StaticMesh = Asset;

        if (!IsValid(StaticMesh) || !FFBXReimportService::IsSupportedMesh(StaticMesh))
        {
            DiagnosticText = TEXT("選択したアセットはStatic MeshまたはSkeletal Meshではありません。");
        }
        else
        {
            FFBXReimportService::TryCreateDraft(StaticMesh, Draft, DiagnosticText);
        }
    }

    const ISlateStyle& Style = FFBXReimportAssistantStyle::Get();
    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);

    Root->AddSlot().AutoHeight().Padding(8.0f)
    [
        SNew(SButton)
        .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Secondary")))
        .Text(FText::FromString(TEXT("← ホームに戻る")))
        .IsEnabled_Lambda([Draft = Draft]()
        {
            return !Draft.IsValid() || !Draft->bReimportInProgress;
        })
        .OnClicked_Lambda([OnBackToHome = Arguments._OnBackToHome]()
        {
            OnBackToHome.ExecuteIfBound();
            return FReply::Handled();
        })
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
                .Text(FText::FromString(TEXT("SINGLE ASSET WORKFLOW")))
                .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("単体アセットの設定・再インポート")))
                .Font(Style.GetFontStyle(TEXT("Font.Title")))
                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
            ]
        ]
    ];

    if (!Draft.IsValid())
    {
        Root->AddSlot()
        .AutoHeight()
        .Padding(12.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(DiagnosticText))
            .AutoWrapText(true)
        ];
    }
    else
    {
        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("対象アセット: %s  [%s]"), *Draft->AssetName, *Draft->MeshTypeLabel)))
            .Font(Style.GetFontStyle(TEXT("Font.Section")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("FBXソース: %s"), *Draft->SourceFilePath)))
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
        ];

        // ---- プリセット ----
        PresetSaveScope = MakeShared<FFBXBatchOverrideSettings>();
        PresetSaveScope->bOverrideTranslation = true;
        PresetSaveScope->bOverrideRotation = true;
        PresetSaveScope->bOverrideUniformScale = true;

        // 保存時に含める項目のチェックボックスを作る。
        auto MakeScopeCheckBox = [Scope = PresetSaveScope](const TCHAR* Label, bool FFBXBatchOverrideSettings::* Member)
        {
            return SNew(SCheckBox)
                .IsChecked_Lambda([Scope, Member]()
                {
                    return (*Scope).*Member ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([Scope, Member](ECheckBoxState NewState)
                {
                    (*Scope).*Member = NewState == ECheckBoxState::Checked;
                })
                [
                    SNew(STextBlock).Text(FText::FromString(Label))
                ];
        };

        Root->AddSlot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("プリセットに保存する項目（読み込み時は保存された項目だけを反映します）")))
                .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                [
                    MakeScopeCheckBox(TEXT("Translation"), &FFBXBatchOverrideSettings::bOverrideTranslation)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                [
                    MakeScopeCheckBox(TEXT("Rotation"), &FFBXBatchOverrideSettings::bOverrideRotation)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    MakeScopeCheckBox(TEXT("Uniform Scale"), &FFBXBatchOverrideSettings::bOverrideUniformScale)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SFBXPresetPanel)
                // 保存: 現在の入力値のうち、チェックした項目をプリセットにする。
                .OnGetSettingsToSave_Lambda(
                    [Draft = Draft, Scope = PresetSaveScope](FFBXBatchOverrideSettings& OutSettings, FString& OutError)
                    {
                        if (!Draft.IsValid() || !Scope.IsValid())
                        {
                            OutError = TEXT("設定データを取得できません。");
                            return false;
                        }

                        if (!Scope->HasAnyOverride())
                        {
                            OutError = TEXT("保存する項目を1つ以上チェックしてください。");
                            return false;
                        }

                        OutSettings = FFBXBatchOverrideSettings();
                        OutSettings.bOverrideTranslation = Scope->bOverrideTranslation;
                        OutSettings.bOverrideRotation = Scope->bOverrideRotation;
                        OutSettings.bOverrideUniformScale = Scope->bOverrideUniformScale;
                        OutSettings.Translation = Draft->Translation;
                        OutSettings.Rotation = Draft->Rotation;
                        OutSettings.UniformScale = Draft->UniformScale;
                        return true;
                    }
                )
                // 読み込み: プリセットに含まれる項目だけを入力値へ反映する。
                // アセットへの適用は、これまで通り「設定を適用」で行う。
                .OnPresetLoaded_Lambda(
                    [Draft = Draft, Status = ApplyStatus](const FFBXImportPreset& Preset, FString& OutMessage)
                    {
                        if (!Draft.IsValid())
                        {
                            OutMessage = TEXT("設定データを取得できません。");
                            return false;
                        }

                        if (Draft->bReimportInProgress)
                        {
                            OutMessage = TEXT("再インポート中はプリセットを読み込めません。");
                            return false;
                        }

                        const FFBXBatchOverrideSettings& Loaded = Preset.Settings;
                        if (!Loaded.HasAnyOverride())
                        {
                            OutMessage = TEXT("このプリセットには反映する項目がありません。");
                            return false;
                        }

                        TArray<FString> AppliedItems;
                        if (Loaded.bOverrideTranslation)
                        {
                            Draft->Translation = Loaded.Translation;
                            AppliedItems.Add(TEXT("Translation"));
                        }
                        if (Loaded.bOverrideRotation)
                        {
                            Draft->Rotation = Loaded.Rotation;
                            AppliedItems.Add(TEXT("Rotation"));
                        }
                        if (Loaded.bOverrideUniformScale)
                        {
                            Draft->UniformScale = Loaded.UniformScale;
                            AppliedItems.Add(TEXT("Uniform Scale"));
                        }

                        Draft->bSettingsApplied = false;
                        *Status = TEXT("未適用の変更があります。");

                        OutMessage = FString::Printf(
                            TEXT("読み込みました：%s\n反映した項目: %s\nプレビューを確認し、「設定を適用」を押してください。"),
                            *Preset.Name,
                            *FString::Join(AppliedItems, TEXT(", ")));
                        return true;
                    }
                )
            ]
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Translation Offset")))
            .Font(Style.GetFontStyle(TEXT("Font.Section")))
            .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
        ];

        TSharedRef<SHorizontalBox> TranslationRow = SNew(SHorizontalBox);
        TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("X")),
                [Draft = Draft]() { return static_cast<float>(Draft->Translation.X); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Translation.X = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Y")),
                [Draft = Draft]() { return static_cast<float>(Draft->Translation.Y); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Translation.Y = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Z")),
                [Draft = Draft]() { return static_cast<float>(Draft->Translation.Z); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Translation.Z = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        Root->AddSlot().AutoHeight()[TranslationRow];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Rotation Offset")))
            .Font(Style.GetFontStyle(TEXT("Font.Section")))
            .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
        ];

        TSharedRef<SHorizontalBox> RotationRow = SNew(SHorizontalBox);
        RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Roll")),
                [Draft = Draft]() { return static_cast<float>(Draft->Rotation.Roll); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Rotation.Roll = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Pitch")),
                [Draft = Draft]() { return static_cast<float>(Draft->Rotation.Pitch); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Rotation.Pitch = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Yaw")),
                [Draft = Draft]() { return static_cast<float>(Draft->Rotation.Yaw); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Rotation.Yaw = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        Root->AddSlot().AutoHeight()[RotationRow];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Uniform Scale")))
            .Font(Style.GetFontStyle(TEXT("Font.Section")))
            .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
        ];

        Root->AddSlot().AutoHeight().Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Scale")),
                [Draft = Draft]() { return static_cast<float>(Draft->UniformScale); },
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->UniformScale = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("変更前 → 変更後のプレビュー")))
            .Font(Style.GetFontStyle(TEXT("Font.Section")))
            .ColorAndOpacity(FSlateColor(Style.GetColor(TEXT("Color.AccentHover"))))
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(SBorder)
            .BorderImage(Style.GetBrush(TEXT("Brush.Surface")))
            .Padding(FMargin(12.0f))
            [
            SNew(STextBlock)
            .Text_Lambda([Draft = Draft]()
            {
                return FText::FromString(BuildDraftPreview(*Draft));
            })
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
            ]
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(SButton)
            .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Primary")))
            .Text(FText::FromString(TEXT("設定を適用")))
            .OnClicked_Lambda([Draft = Draft, Status = ApplyStatus]()
            {
                // Skeletal Meshはスケルトンやアニメーションに影響するため、適用前に確認する。
                if (Draft.IsValid() && Draft->bIsSkeletalMesh)
                {
                    const EAppReturnType::Type Response = FMessageDialog::Open(
                        EAppMsgType::YesNo,
                        FText::FromString(TEXT(
                            "Skeletal Meshのオフセット（Translation / Rotation / Scale）を変更すると、\n"
                            "このメッシュのスケルトンを使用するアニメーションの見え方がずれる場合があります。\n\n"
                            "設定を適用しますか？")));

                    if (Response != EAppReturnType::Yes)
                    {
                        *Status = TEXT("設定の適用をキャンセルしました。");
                        return FReply::Handled();
                    }
                }

                FString Message;
                FFBXReimportService::ApplyDraftSettings(Draft, Message);
                *Status = Message;
                return FReply::Handled();
            })
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([Status = ApplyStatus]() { return FText::FromString(*Status); })
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(SButton)
            .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Success")))
            .Text(FText::FromString(TEXT("FBXを再インポート")))
            .IsEnabled_Lambda([Draft = Draft]()
            {
                return Draft.IsValid() && Draft->bSettingsApplied && !Draft->bReimportInProgress;
            })
            .OnClicked_Lambda([Draft = Draft, Status = ReimportStatus]()
            {
                const EAppReturnType::Type Response = FMessageDialog::Open(
                    EAppMsgType::YesNo,
                    FText::FromString(FString::Printf(
                        TEXT("以下のアセットを再インポートします。\n\n%s\n%s\n\n続行しますか？"),
                        *Draft->AssetName, *Draft->SourceFilePath)));

                if (Response != EAppReturnType::Yes)
                {
                    *Status = TEXT("再インポートをキャンセルしました。");
                    return FReply::Handled();
                }

                FString Error;
                const bool bStarted = FFBXReimportService::StartReimportAsync(
                    Draft,
                    [Draft, Status](bool bSuccess, int32 ImportedCount, const FString& Message)
                    {
                        Draft->bReimportInProgress = false;
                        Draft->bSettingsApplied = false;
                        *Status = FString::Printf(
                            TEXT("%s\n対象: %s\n結果オブジェクト数: %d\n\n最新情報を取得するにはウィンドウを開き直してください。"),
                            *Message, *Draft->AssetName, ImportedCount);
                    },
                    Error);

                if (!bStarted)
                {
                    *Status = Error;
                }
                else
                {
                    *Status = TEXT("再インポートを開始しました...");
                }

                return FReply::Handled();
            })
        ];

        Root->AddSlot().AutoHeight().Padding(8.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([Status = ReimportStatus]() { return FText::FromString(*Status); })
            .Font(Style.GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
            .AutoWrapText(true)
        ];
    }

    Root->AddSlot().AutoHeight().Padding(10.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("このツールはFBXソースを持つInterchange対応のStatic Mesh / Skeletal Meshを対象とします。")))
        .Font(Style.GetFontStyle(TEXT("Font.Caption")))
        .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextMuted")))
        .AutoWrapText(true)
    ];

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Background")))
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                Root
            ]
        ]
    ];
}
