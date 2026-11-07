#include "Widgets/SFBXReimportHomeWindow.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SFBXReimportHomeWindow::Construct(const FArguments& Arguments)
{
    ChildSlot
    [
        SNew(SBorder)
        .Padding(18.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 4.0f, 4.0f, 10.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("FBX Reimport Assistant")))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 0.0f, 4.0f, 18.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("目的に合わせてツールを選択してください。")))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SButton)
                .ContentPadding(FMargin(14.0f, 12.0f))
                .OnClicked_Lambda([OpenSingle = Arguments._OnOpenSingleTool]()
                {
                    OpenSingle.ExecuteIfBound();
                    return FReply::Handled();
                })
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("単体アセットの設定・再インポート")))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 5.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(
                            "Static Meshを1つ選択して、FBXのTranslation Offset・Rotation Offset・Uniform Scaleを確認・変更します。\n"
                            "設定を個別に調整してから、そのアセットだけを再インポートしたい場合に使用します。"
                        )))
                        .AutoWrapText(true)
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 10.0f, 4.0f, 4.0f)
            [
                SNew(SButton)
                .ContentPadding(FMargin(14.0f, 12.0f))
                .OnClicked_Lambda([OpenBatch = Arguments._OnOpenBatchTool]()
                {
                    OpenBatch.ExecuteIfBound();
                    return FReply::Handled();
                })
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("複数アセットの一括設定・再インポート")))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 5.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(
                            "複数のStatic Meshを選択して、Translation Offset・Rotation Offset・Uniform Scaleの指定項目を共通値に変更します。\n"
                            "複数アセットをまとめて再インポートしたい場合に使用します。チェックしていない設定は各アセットの値を維持します。"
                        )))
                        .AutoWrapText(true)
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 18.0f, 4.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT(
                    "使用前にコンテンツブラウザで対象のStatic Meshを選択してください。\n"
                    "単体ツールは1つ、一括ツールは複数のアセットを選択してから開きます。"
                )))
                .AutoWrapText(true)
            ]
        ]
    ];
}
