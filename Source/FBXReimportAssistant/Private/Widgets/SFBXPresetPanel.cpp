#include "Widgets/SFBXPresetPanel.h"

#include "Services/FBXImportPresetService.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"

void SFBXPresetPanel::Construct(const FArguments& InArgs)
{
    OnGetSettingsToSave = InArgs._OnGetSettingsToSave;
    OnPresetLoaded = InArgs._OnPresetLoaded;

    ChildSlot
    [
        SNew(SBorder)
        .Padding(10)
        [
            SNew(SVerticalBox)

            // セクション見出し
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("インポート設定プリセット")))
            ]

            // プリセット名入力欄
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SAssignNew(PresetNameTextBox, SEditableTextBox)
                .HintText(FText::FromString(TEXT("プリセット名を入力")))
            ]

            // プリセット選択欄
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SAssignNew(PresetComboBox, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&PresetOptions)
                .OnGenerateWidget_Lambda(
                    [](TSharedPtr<FString> Item)
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(FPaths::GetBaseFilename(*Item)));
                    }
                )
                .OnSelectionChanged_Lambda(
                    [this](TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
                    {
                        SelectedPresetFilePath = NewSelection.IsValid()
                            ? *NewSelection
                            : FString();
                    }
                )
                [
                    SNew(STextBlock)
                    .Text_Lambda(
                        [this]()
                        {
                            if (!SelectedPresetFilePath.IsEmpty())
                            {
                                return FText::FromString(
                                    FPaths::GetBaseFilename(SelectedPresetFilePath));
                            }

                            return FText::FromString(TEXT("プリセットを選択"));
                        }
                    )
                ]
            ]

            // プリセット保存ボタン
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("現在の設定を新規保存")))
                .OnClicked(this, &SFBXPresetPanel::OnSavePresetClicked)
            ]

            // プリセット読み込みボタン
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("選択したプリセットを読み込む")))
                .OnClicked(this, &SFBXPresetPanel::OnLoadPresetClicked)
            ]

            // プリセット削除ボタン
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("選択したプリセットを削除")))
                .OnClicked(this, &SFBXPresetPanel::OnDeletePresetClicked)
            ]

            // 状態表示
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(PresetStatusTextBlock, STextBlock)
                .Text(FText::FromString(TEXT("プリセットを選択してください。")))
                .AutoWrapText(true)
            ]
        ]
    ];

    RefreshPresetList();
}

void SFBXPresetPanel::SetStatus(const FString& Message)
{
    if (PresetStatusTextBlock.IsValid())
    {
        PresetStatusTextBlock->SetText(FText::FromString(Message));
    }
}

void SFBXPresetPanel::RefreshPresetList()
{
    PresetOptions.Reset();

    TArray<FString> FilePaths;
    FString Message;

    const bool bSuccess = FBXImportPresetService::GetPresetFiles(FilePaths, Message);

    if (!bSuccess)
    {
        SetStatus(Message);
        return;
    }

    for (const FString& FilePath : FilePaths)
    {
        PresetOptions.Add(MakeShared<FString>(FilePath));
    }

    if (PresetComboBox.IsValid())
    {
        PresetComboBox->RefreshOptions();
        PresetComboBox->ClearSelection();
    }

    SelectedPresetFilePath.Empty();

    SetStatus(Message);
}

FReply SFBXPresetPanel::OnSavePresetClicked()
{
    if (!PresetNameTextBox.IsValid() || !PresetStatusTextBlock.IsValid())
    {
        return FReply::Handled();
    }

    if (!OnGetSettingsToSave.IsBound())
    {
        SetStatus(TEXT("設定データを取得できません。"));
        return FReply::Handled();
    }

    // 呼び出し元から、保存する設定を取得する。
    FFBXBatchOverrideSettings SettingsToSave;
    FString SettingsError;
    if (!OnGetSettingsToSave.Execute(SettingsToSave, SettingsError))
    {
        SetStatus(SettingsError.IsEmpty()
            ? FString(TEXT("設定データを取得できません。"))
            : SettingsError);
        return FReply::Handled();
    }

    FFBXImportPreset Preset;
    Preset.Name = PresetNameTextBox->GetText().ToString();
    Preset.Version = 1;
    Preset.Settings = SettingsToSave;

    FString SavedFilePath;
    FString Message;

    // 上書きを許可せずに保存を試す
    bool bSuccess = FBXImportPresetService::SavePreset(
        Preset,
        false,
        SavedFilePath,
        Message
    );

    if (!bSuccess)
    {
        // 同名プリセットが存在する場合だけ上書き確認
        if (Message == TEXT("同名のプリセットが既に存在します。"))
        {
            const EAppReturnType::Type Result = FMessageDialog::Open(
                EAppMsgType::YesNo,
                FText::FromString(FString::Printf(
                    TEXT("プリセット「%s」は既に存在します。\n上書きしますか？"),
                    *Preset.Name))
            );

            if (Result != EAppReturnType::Yes)
            {
                SetStatus(TEXT("上書きをキャンセルしました。"));
                return FReply::Handled();
            }

            // 上書きを許可して保存
            bSuccess = FBXImportPresetService::SavePreset(
                Preset,
                true,
                SavedFilePath,
                Message
            );
        }

        if (!bSuccess)
        {
            SetStatus(Message);
            return FReply::Handled();
        }
    }

    RefreshPresetList();

    SetStatus(FString::Printf(
        TEXT("保存しました：%s"),
        *FPaths::GetBaseFilename(SavedFilePath)));

    return FReply::Handled();
}

FReply SFBXPresetPanel::OnLoadPresetClicked()
{
    if (SelectedPresetFilePath.IsEmpty())
    {
        SetStatus(TEXT("読み込むプリセットを選択してください。"));
        return FReply::Handled();
    }

    // LoadedPresetに、JSONから復元した内容が入る
    FFBXImportPreset LoadedPreset;
    FString Message;

    const bool bSuccess = FBXImportPresetService::LoadPreset(
        SelectedPresetFilePath,
        LoadedPreset,
        Message
    );

    if (!bSuccess)
    {
        SetStatus(Message);
        return FReply::Handled();
    }

    if (!OnPresetLoaded.IsBound())
    {
        SetStatus(TEXT("プリセットの反映先を取得できません。"));
        return FReply::Handled();
    }

    // 呼び出し元の画面へ反映する。
    FString ApplyMessage;
    const bool bApplied = OnPresetLoaded.Execute(LoadedPreset, ApplyMessage);

    if (!bApplied)
    {
        SetStatus(ApplyMessage.IsEmpty()
            ? FString(TEXT("プリセットを反映できませんでした。"))
            : ApplyMessage);
        return FReply::Handled();
    }

    SetStatus(ApplyMessage.IsEmpty()
        ? FString::Printf(TEXT("読み込みました：%s"), *LoadedPreset.Name)
        : ApplyMessage);

    return FReply::Handled();
}

FReply SFBXPresetPanel::OnDeletePresetClicked()
{
    if (SelectedPresetFilePath.IsEmpty())
    {
        SetStatus(TEXT("削除するプリセットを選択してください。"));
        return FReply::Handled();
    }

    const FString PresetName = FPaths::GetBaseFilename(SelectedPresetFilePath);

    const EAppReturnType::Type Result = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(FString::Printf(
            TEXT("プリセット「%s」を削除しますか？"), *PresetName))
    );

    if (Result != EAppReturnType::Yes)
    {
        return FReply::Handled();
    }

    FString Message;
    const bool bSuccess = FBXImportPresetService::DeletePreset(PresetName, Message);

    if (!bSuccess)
    {
        SetStatus(Message);
        return FReply::Handled();
    }

    RefreshPresetList();
    SetStatus(Message);
    return FReply::Handled();
}
