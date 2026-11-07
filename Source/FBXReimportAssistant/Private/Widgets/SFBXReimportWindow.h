#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "FBXReimportTypes.h"

class SFBXReimportWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFBXReimportWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& Arguments);

private:
    static TSharedRef<SVerticalBox> MakeNumericField(
        const FText& Label,
        float InitialValue,
        TFunction<void(float)> OnValueChanged
    );

    TSharedPtr<FFBXImportSettingsDraft> Draft;
    TSharedPtr<FString> ApplyStatus;
    TSharedPtr<FString> ReimportStatus;
};
