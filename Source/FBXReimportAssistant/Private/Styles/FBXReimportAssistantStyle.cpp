#include "Styles/FBXReimportAssistantStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/AppStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FFBXReimportAssistantStyle::StyleInstance;
bool FFBXReimportAssistantStyle::bHasPluginIcon = false;

FSlateIcon FFBXReimportAssistantStyle::GetToolbarIcon()
{
    return bHasPluginIcon && StyleInstance.IsValid()
        ? FSlateIcon(StyleInstance->GetStyleSetName(), TEXT("Icon.Toolbar"))
        : FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh"));
}

FSlateIcon FFBXReimportAssistantStyle::GetMenuIcon()
{
    return bHasPluginIcon && StyleInstance.IsValid()
        ? FSlateIcon(StyleInstance->GetStyleSetName(), TEXT("Icon.Menu"))
        : FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh"));
}

void FFBXReimportAssistantStyle::Initialize()
{
    if (StyleInstance.IsValid())
    {
        return;
    }

    StyleInstance = MakeShared<FSlateStyleSet>(FName(TEXT("FBXReimportAssistantStyle")));

    // UE Editor標準のAppStyleを親として利用します。
    // 独自の色・ボタン表現でエディタの見た目を上書きせず、標準テーマに追従させます。
    StyleInstance->SetParentStyleName(FAppStyle::GetAppStyleSetName());

    const ISlateStyle& AppStyle = FAppStyle::Get();

    // ------------------------------------------------------------
    // 背景・パネル
    // UE標準のパネルブラシを別名で参照し、既存Widgetの構造は維持します。
    // ------------------------------------------------------------
    StyleInstance->Set(
        TEXT("Brush.Background"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Surface"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Hero"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Card"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.CardHover"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.CardPressed"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Badge"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
    );

    // ロゴ領域も独自の強い塗りつぶしを避け、UE標準パネル表現に合わせます。
    StyleInstance->Set(
        TEXT("Brush.Accent"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.AccentHover"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Success"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.SuccessHover"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Warning"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Danger"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.GroupBorder")))
    );
    StyleInstance->Set(
        TEXT("Brush.Divider"),
        new FSlateBrush(*AppStyle.GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
    );

    // ------------------------------------------------------------
    // 文字色
    // UseForeground / UseSubduedForegroundを使うことで、UE Editor側の
    // テーマ設定に応じた文字色を使用します。
    // ------------------------------------------------------------
    StyleInstance->Set(TEXT("Color.TextPrimary"), FSlateColor::UseForeground());
    StyleInstance->Set(TEXT("Color.TextSecondary"), FSlateColor::UseSubduedForeground());
    StyleInstance->Set(TEXT("Color.TextMuted"), FSlateColor::UseSubduedForeground());

    // アクセント色は用途を識別する場所に限定します。
    // UE標準UIの明るさを邪魔しない、控えめな色です。
    StyleInstance->Set(TEXT("Color.Accent"), FLinearColor(0.24f, 0.43f, 0.58f, 1.0f));
    StyleInstance->Set(TEXT("Color.AccentHover"), FLinearColor(0.34f, 1.0f, 1.0f, 1.0f));
    StyleInstance->Set(TEXT("Color.Success"), FLinearColor(0.30f, 1.0f, 0.39f, 1.0f));
    StyleInstance->Set(TEXT("Color.SuccessHover"), FLinearColor(0.38f, 1.0f, 0.1f, 1.0f));
    StyleInstance->Set(TEXT("Color.Warning"), FLinearColor(0.62f, 0.48f, 0.27f, 1.0f));
    StyleInstance->Set(TEXT("Color.Danger"), FLinearColor(0.62f, 0.31f, 0.31f, 1.0f));

    // ------------------------------------------------------------
    // フォント
    // UE標準のNormalTextをベースにサイズだけを変えます。
    // 8pxなど小さすぎるキャプションは避け、読みやすさを優先します。
    // ------------------------------------------------------------
    const auto MakeEditorFont = [&AppStyle](int32 Size, const FName Typeface)
    {
        FSlateFontInfo Font = AppStyle.GetFontStyle(TEXT("NormalFont"));
        Font.Size = Size;
        Font.TypefaceFontName = Typeface;
        return Font;
    };

    StyleInstance->Set(TEXT("Font.Display"), MakeEditorFont(22, FName(TEXT("Bold"))));
    StyleInstance->Set(TEXT("Font.Title"), MakeEditorFont(16, FName(TEXT("Bold"))));
    StyleInstance->Set(TEXT("Font.Section"), MakeEditorFont(12, FName(TEXT("Bold"))));
    StyleInstance->Set(TEXT("Font.Body"), MakeEditorFont(10, FName(TEXT("Regular"))));
    StyleInstance->Set(TEXT("Font.Caption"), MakeEditorFont(9, FName(TEXT("Regular"))));
    StyleInstance->Set(TEXT("Font.CardNumber"), MakeEditorFont(18, FName(TEXT("Bold"))));

    // ------------------------------------------------------------
    // ボタン
    // UE標準のButtonスタイルを利用します。
    // 操作ごとの独自塗り分けは避け、UE Editorと同じホバー・押下表現に統一します。
    // ------------------------------------------------------------
    const FButtonStyle& EditorButton = AppStyle.GetWidgetStyle<FButtonStyle>(TEXT("Button"));

    FButtonStyle CardButton = EditorButton;
    CardButton.SetNormalPadding(FMargin(0.0f));
    CardButton.SetPressedPadding(FMargin(0.0f));
    StyleInstance->Set(TEXT("Button.Card"), CardButton);

    FButtonStyle StandardButton = EditorButton;
    StandardButton.SetNormalPadding(FMargin(10.0f, 6.0f));
    StandardButton.SetPressedPadding(FMargin(10.0f, 6.0f));
    StyleInstance->Set(TEXT("Button.Primary"), StandardButton);
    StyleInstance->Set(TEXT("Button.Success"), StandardButton);
    StyleInstance->Set(TEXT("Button.Secondary"), StandardButton);

    // ------------------------------------------------------------
    // プラグインアイコン
    // <プラグイン>/Resources/Icon128.png をホーム画面のロゴとして使用する。
    // モジュール名からプラグインの場所を特定し、ファイルが無い場合は登録しない
    // （表示側で従来の「FBX」文字にフォールバックする）。
    // ------------------------------------------------------------
    {
        TSharedPtr<IPlugin> OwnerPlugin =
            IPluginManager::Get().GetModuleOwnerPlugin(TEXT("FBXReimportAssistant"));

        if (OwnerPlugin.IsValid())
        {
            const FString IconPath = OwnerPlugin->GetBaseDir() / TEXT("Resources") / TEXT("Icon128.png");

            if (FPaths::FileExists(IconPath))
            {
                StyleInstance->Set(
                    TEXT("Brush.PluginIcon"),
                    new FSlateImageBrush(IconPath, FVector2D(64.0f, 64.0f))
                );

                // レベルエディタのツールバー（20px）とメニュー（16px）用。
                StyleInstance->Set(
                    TEXT("Icon.Toolbar"),
                    new FSlateImageBrush(IconPath, FVector2D(20.0f, 20.0f))
                );
                StyleInstance->Set(
                    TEXT("Icon.Menu"),
                    new FSlateImageBrush(IconPath, FVector2D(16.0f, 16.0f))
                );

                bHasPluginIcon = true;
            }
        }
    }

    FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FFBXReimportAssistantStyle::Shutdown()
{
    if (!StyleInstance.IsValid())
    {
        return;
    }

    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    StyleInstance.Reset();
    bHasPluginIcon = false;
}

const ISlateStyle& FFBXReimportAssistantStyle::Get()
{
    return StyleInstance.IsValid() ? static_cast<const ISlateStyle&>(*StyleInstance) : FAppStyle::Get();
}
