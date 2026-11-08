#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FBXReimportTypes.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"

class FAssetThumbnail;
class FAssetThumbnailPool;

class SFBXBatchReimportWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFBXBatchReimportWindow) {}
        SLATE_EVENT(FSimpleDelegate, OnBackToHome)
    SLATE_END_ARGS()

    void Construct(const FArguments& Arguments);

private:
    TSharedPtr<FFBXBatchState> BatchState;

    // 検索結果として表示するアセット。元データはBatchState->Itemsで保持する。
    TArray<TSharedPtr<FFBXBatchItem>> FilteredItems;
    TSharedPtr<SListView<TSharedPtr<FFBXBatchItem>>> AssetListView;

    // サムネイル描画の負荷を管理し、生成したThumbnailの寿命を保持する。
    TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
    TMap<const FFBXBatchItem*, TSharedPtr<FAssetThumbnail>> ThumbnailByItem;

    FString SearchText;

    void RefreshFilteredAssetList();
    void SetAllAssetSelection(bool bSelected);

    TSharedRef<ITableRow> OnGenerateAssetRow(
        TSharedPtr<FFBXBatchItem> Item,
        const TSharedRef<STableViewBase>& OwnerTable
    );
    
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
    
    // プリセットパネルを生成する
    TSharedRef<SWidget> BuildPresetPanel();
    
    // 保存済みプリセット一覧を更新する
    void RefreshPresetList();
    
    // ボタンがクリックされたときに実行する関数
    FReply OnSavePresetClicked();
    FReply OnLoadPresetClicked();
    FReply OnDeletePresetClicked();
};
