#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FBXReimportTypes.h"

class SFBXBatchReimportWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFBXBatchReimportWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& Arguments);

private:
    TSharedPtr<FFBXBatchState> BatchState;
};
