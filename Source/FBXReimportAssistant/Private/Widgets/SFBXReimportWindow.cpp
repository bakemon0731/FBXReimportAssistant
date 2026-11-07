#include "Widgets/SFBXReimportWindow.h"

#include "Services/FBXReimportService.h"
#include "Styles/FBXReimportAssistantStyle.h"
#include "Styling/SlateTypes.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Misc/MessageDialog.h"

TSharedRef<SVerticalBox> SFBXReimportWindow::MakeNumericField(
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
            .Font(FFBXReimportAssistantStyle::Get().GetFontStyle(TEXT("Font.Caption")))
            .ColorAndOpacity(FFBXReimportAssistantStyle::Get().GetSlateColor(TEXT("Color.TextSecondary")))
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
        DiagnosticText = TEXT("コンテンツブラウザでStatic Meshを1つだけ選択してください。");
    }
    else
    {
        UObject* Asset = SelectedAssets[0].GetAsset();
        UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);

        if (!IsValid(StaticMesh))
        {
            DiagnosticText = TEXT("選択したアセットはStatic Meshではありません。");
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
            .Text(FText::FromString(FString::Printf(TEXT("対象アセット: %s"), *Draft->AssetName)))
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
            MakeNumericField(FText::FromString(TEXT("X")), Draft->Translation.X,
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Translation.X = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Y")), Draft->Translation.Y,
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Translation.Y = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        TranslationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Z")), Draft->Translation.Z,
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
            MakeNumericField(FText::FromString(TEXT("Roll")), Draft->Rotation.Roll,
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Rotation.Roll = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Pitch")), Draft->Rotation.Pitch,
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->Rotation.Pitch = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
        ];
        RotationRow->AddSlot().FillWidth(1.0f).Padding(4.0f)
        [
            MakeNumericField(FText::FromString(TEXT("Yaw")), Draft->Rotation.Yaw,
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
            MakeNumericField(FText::FromString(TEXT("Scale")), Draft->UniformScale,
                [Draft = Draft, Status = ApplyStatus](float Value)
                {
                    Draft->UniformScale = Value;
                    Draft->bSettingsApplied = false;
                    *Status = TEXT("未適用の変更があります。");
                })
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
                return FText::FromString(FString::Printf(
                    TEXT("現在の一時データ\nTranslation: %.2f / %.2f / %.2f\nRotation: %.2f / %.2f / %.2f\nUniform Scale: %.2f"),
                    Draft->Translation.X, Draft->Translation.Y, Draft->Translation.Z,
                    Draft->Rotation.Roll, Draft->Rotation.Pitch, Draft->Rotation.Yaw,
                    Draft->UniformScale));
            })
            .Font(Style.GetFontStyle(TEXT("Font.Body")))
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
        .Text(FText::FromString(TEXT("このツールはFBXソースを持つInterchange対応Static Meshを対象とします。")))
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
