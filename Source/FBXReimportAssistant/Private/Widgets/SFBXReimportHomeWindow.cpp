#include "Widgets/SFBXReimportHomeWindow.h"

#include "Styles/FBXReimportAssistantStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    TSharedRef<SWidget> MakeHomeToolCard(
        const ISlateStyle& Style,
        const FText& Number,
        const FText& EnglishLabel,
        const FText& Title,
        const FText& Description,
        const FText& ActionLabel,
        const FSlateBrush* IconBrush,
        const FLinearColor& AccentColor,
        FSimpleDelegate OnClicked
    )
    {
        return SNew(SButton)
            .ButtonStyle(&Style.GetWidgetStyle<FButtonStyle>(TEXT("Button.Card")))
            .ContentPadding(FMargin(18.0f))
            .OnClicked_Lambda([OnClicked]()
            {
                OnClicked.ExecuteIfBound();
                return FReply::Handled();
            })
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 9.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBorder)
                        .BorderImage(Style.GetBrush(TEXT("Brush.Badge")))
                        .Padding(FMargin(9.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(34.0f)
                            .HeightOverride(34.0f)
                            [
                                SNew(SImage)
                                .Image(IconBrush)
                                .ColorAndOpacity(FSlateColor(AccentColor))
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .Padding(10.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(EnglishLabel)
                        .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                        .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextMuted")))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(Number)
                        .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                        .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextMuted")))
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(STextBlock)
                    .Text(Title)
                    .Font(Style.GetFontStyle(TEXT("Font.Title")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                    .AutoWrapText(true)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 14.0f)
                [
                    SNew(STextBlock)
                    .Text(Description)
                    .Font(Style.GetFontStyle(TEXT("Font.Body")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                    .AutoWrapText(true)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(ActionLabel)
                        .Font(Style.GetFontStyle(TEXT("Font.Section")))
                        .ColorAndOpacity(FSlateColor(AccentColor))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("→")))
                        .Font(Style.GetFontStyle(TEXT("Font.Title")))
                        .ColorAndOpacity(FSlateColor(AccentColor))
                    ]
                ]
            ];
    }
}

void SFBXReimportHomeWindow::Construct(const FArguments& Arguments)
{
    const ISlateStyle& Style = FFBXReimportAssistantStyle::Get();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(Style.GetBrush(TEXT("Brush.Background")))
        .Padding(0.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(22.0f)
            [
                SNew(SVerticalBox)
                // Hero header
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 18.0f)
                [
                    SNew(SBorder)
                    .BorderImage(Style.GetBrush(TEXT("Brush.Hero")))
                    .Padding(FMargin(20.0f, 18.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBorder)
                            .BorderImage(Style.GetBrush(TEXT("Brush.Accent")))
                            .Padding(FMargin(12.0f, 13.0f))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("FBX")))
                                .Font(Style.GetFontStyle(TEXT("Font.Title")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        .Padding(16.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("FBX Reimport Assistant")))
                                .Font(Style.GetFontStyle(TEXT("Font.Display")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                            ]
                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("ASSET PIPELINE TOOLKIT  /  UNREAL ENGINE")))
                                .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Top)
                        [
                            SNew(SBorder)
                            .BorderImage(Style.GetBrush(TEXT("Brush.Badge")))
                            .Padding(FMargin(10.0f, 6.0f))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("EDITOR TOOL")))
                                .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                            ]
                        ]
                    ]
                ]
                // Introduction
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2.0f, 0.0f, 2.0f, 12.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("目的からツールを選択")))
                    .Font(Style.GetFontStyle(TEXT("Font.Title")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2.0f, 0.0f, 2.0f, 16.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("アセットの数や作業内容に合わせて、使用する機能を選んでください。")))
                    .Font(Style.GetFontStyle(TEXT("Font.Body")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                    .AutoWrapText(true)
                ]
                // Tool cards
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(0.0f, 0.0f, 7.0f, 0.0f)
                    [
                        MakeHomeToolCard(
                            Style,
                            FText::FromString(TEXT("01")),
                            FText::FromString(TEXT("SINGLE ASSET")),
                            FText::FromString(TEXT("単体アセットの設定・再インポート")),
                            FText::FromString(TEXT("Static Meshを1つ選択し、Translation Offset・Rotation Offset・Uniform Scaleを個別に確認・変更します。1つのアセットだけを調整したい場合に使用します。")),
                            FText::FromString(TEXT("単体ツールを開く")),
                            FAppStyle::Get().GetBrush(TEXT("ClassIcon.StaticMesh")),
                            Style.GetColor(TEXT("Color.AccentHover")),
                            Arguments._OnOpenSingleTool
                        )
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(7.0f, 0.0f, 0.0f, 0.0f)
                    [
                        MakeHomeToolCard(
                            Style,
                            FText::FromString(TEXT("02")),
                            FText::FromString(TEXT("BATCH ASSETS")),
                            FText::FromString(TEXT("複数アセットの一括設定・再インポート")),
                            FText::FromString(TEXT("複数のStatic Meshに共通設定を適用して、順番に再インポートします。チェックしていない設定は各アセットの元の値を維持します。")),
                            FText::FromString(TEXT("一括ツールを開く")),
                            FAppStyle::Get().GetBrush(TEXT("Icons.Refresh")),
                            Style.GetColor(TEXT("Color.Success")),
                            Arguments._OnOpenBatchTool
                        )
                    ]
                ]
                // Workflow information
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 18.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(Style.GetBrush(TEXT("Brush.Surface")))
                    .Padding(FMargin(16.0f, 13.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("基本的な作業フロー")))
                            .Font(Style.GetFontStyle(TEXT("Font.Section")))
                            .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextPrimary")))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("01  アセットを選択")))
                                .Font(Style.GetFontStyle(TEXT("Font.Body")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("02  設定を確認・変更")))
                                .Font(Style.GetFontStyle(TEXT("Font.Body")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("03  再インポート")))
                                .Font(Style.GetFontStyle(TEXT("Font.Body")))
                                .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextSecondary")))
                            ]
                        ]
                    ]
                ]
                // Footer note
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(2.0f, 16.0f, 2.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("使用前にコンテンツブラウザで対象のStatic Meshを選択してください。単体ツールは1つ、一括ツールは複数のアセットを選択してから開きます。")))
                    .Font(Style.GetFontStyle(TEXT("Font.Caption")))
                    .ColorAndOpacity(Style.GetSlateColor(TEXT("Color.TextMuted")))
                    .AutoWrapText(true)
                ]
            ]
        ]
    ];
}
