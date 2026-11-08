#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Textures/SlateIcon.h"

/** FBX Reimport Assistant全体で共有するSlateスタイル。 */
class FFBXReimportAssistantStyle
{
public:
    static void Initialize();
    static void Shutdown();

    static const ISlateStyle& Get();

    // ツールバー・メニュー用のアイコン。Icon128.pngが無い場合はStatic Meshの標準アイコンを返す。
    static FSlateIcon GetToolbarIcon();
    static FSlateIcon GetMenuIcon();

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
    static bool bHasPluginIcon;
};
