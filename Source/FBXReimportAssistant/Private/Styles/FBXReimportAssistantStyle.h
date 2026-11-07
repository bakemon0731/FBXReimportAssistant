#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** FBX Reimport Assistant全体で共有するSlateスタイル。 */
class FFBXReimportAssistantStyle
{
public:
    static void Initialize();
    static void Shutdown();

    static const ISlateStyle& Get();

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
};
