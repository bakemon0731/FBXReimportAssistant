#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "FBXReimportTypes.h"

class SFBXReimportWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFBXReimportWindow) {}
        SLATE_EVENT(FSimpleDelegate, OnBackToHome)
    SLATE_END_ARGS()

    void Construct(const FArguments& Arguments);

private:
    // GetValueで常に現在値を読み取るため、プリセット読み込みなど
    // ウィンドウ外からの値の変更も入力欄に反映される。
    static TSharedRef<SVerticalBox> MakeNumericField(
        const FText& Label,
        TFunction<float()> GetValue,
        TFunction<void(float)> OnValueChanged
    );

    TSharedPtr<FFBXImportSettingsDraft> Draft;
    TSharedPtr<FString> ApplyStatus;
    TSharedPtr<FString> ReimportStatus;

    // プリセット保存時に含める項目。bOverride〜をそのまま「含める」の意味で使う。
    // 一括再インポートのプリセットと同じ形式で保存するため、同じ構造体を流用している。
    TSharedPtr<FFBXBatchOverrideSettings> PresetSaveScope;
};
