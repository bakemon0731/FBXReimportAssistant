#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FBXReimportTypes.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"

class STextBlock;

// 保存するプリセットの内容を、呼び出し元から取得する。
// 保存できない場合はfalseを返し、OutErrorに理由を入れる。
DECLARE_DELEGATE_RetVal_TwoParams(
    bool,
    FOnFBXPresetSettingsRequested,
    FFBXBatchOverrideSettings& /*OutSettings*/,
    FString& /*OutError*/
);

// 読み込んだプリセットを、呼び出し元の画面へ反映する。
// 反映できない場合はfalseを返す。OutMessageは状態表示に使われる（空なら既定の文言）。
DECLARE_DELEGATE_RetVal_TwoParams(
    bool,
    FOnFBXPresetLoaded,
    const FFBXImportPreset& /*Preset*/,
    FString& /*OutMessage*/
);

// プリセットの保存・読み込み・削除を行う共通パネル。
// 単体再インポートと一括再インポートの両方で使用する。
class SFBXPresetPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFBXPresetPanel) {}
        SLATE_EVENT(FOnFBXPresetSettingsRequested, OnGetSettingsToSave)
        SLATE_EVENT(FOnFBXPresetLoaded, OnPresetLoaded)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FOnFBXPresetSettingsRequested OnGetSettingsToSave;
    FOnFBXPresetLoaded OnPresetLoaded;

    // プリセット名の入力欄
    TSharedPtr<SEditableTextBox> PresetNameTextBox;

    // プリセット選択用コンボボックス
    TSharedPtr<SComboBox<TSharedPtr<FString>>> PresetComboBox;

    // プリセットファイルの一覧
    TArray<TSharedPtr<FString>> PresetOptions;

    // 現在選択されているファイルパス
    FString SelectedPresetFilePath;

    // プリセットの状態表示
    TSharedPtr<STextBlock> PresetStatusTextBlock;

    void SetStatus(const FString& Message);

    // 保存済みプリセット一覧を更新する
    void RefreshPresetList();

    FReply OnSavePresetClicked();
    FReply OnLoadPresetClicked();
    FReply OnDeletePresetClicked();
};
