#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SFBXReimportHomeWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFBXReimportHomeWindow) {}
        SLATE_EVENT(FSimpleDelegate, OnOpenSingleTool)
        SLATE_EVENT(FSimpleDelegate, OnOpenBatchTool)
    SLATE_END_ARGS()

    void Construct(const FArguments& Arguments);
};
