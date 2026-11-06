
#include "Modules/ModuleManager.h"

// =====================================
// ToolMenus
// =====================================
#include "ToolMenus.h"

// =====================================
// Slate UI
// =====================================
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"

// =====================================
// Transaction
// =====================================
#include "ScopedTransaction.h"

// =====================================
// Content Browser
// =====================================
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

// =====================================
// Asset
// =====================================
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"

// =====================================
// Import Data
// =====================================
#include "EditorFramework/AssetImportData.h"

// =====================================
// Interchange
// =====================================
#include "InterchangeAssetImportData.h"
#include "InterchangeGenericAssetsPipeline.h"


// =====================================
// Import Settings Draft
// UI編集用の一時データ
// =====================================

struct FFBXImportSettingsDraft
{
    FString AssetName;

    FString SourceFilePath;

    FVector Translation = FVector::ZeroVector;

    FRotator Rotation = FRotator::ZeroRotator;

    float UniformScale = 1.0f;

    // 適用対象
    TWeakObjectPtr<UStaticMesh> TargetMesh;

    TWeakObjectPtr<UInterchangeAssetImportData> ImportData;
};


class FFBXReimportAssistantModule : public IModuleInterface
{
public:

    // =====================================
    // Module Startup
    // =====================================

    virtual void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(
                this,
                &FFBXReimportAssistantModule::RegisterMenus
            )
        );
    }


    // =====================================
    // Module Shutdown
    // =====================================

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);

        UToolMenus::UnregisterOwner(this);
    }


private:

    // =====================================
    // Register Editor Menu
    // =====================================

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);

        UToolMenu* Menu =
            UToolMenus::Get()->ExtendMenu(
                "LevelEditor.MainMenu.Tools"
            );

        FToolMenuSection& Section =
            Menu->FindOrAddSection(
                "FBXReimportAssistant"
            );

        Section.AddMenuEntry(
            "OpenFBXReimportAssistant",

            FText::FromString(
                TEXT("FBX Reimport Assistant")
            ),

            FText::FromString(
                TEXT("Open FBX Reimport Assistant Window")
            ),

            FSlateIcon(),

            FUIAction(
                FExecuteAction::CreateRaw(
                    this,
                    &FFBXReimportAssistantModule::OpenWindow
                )
            )
        );
    }


    // =====================================
    // Create Numeric Input Field
    // =====================================

    static TSharedRef<SVerticalBox> MakeNumericField(
        const FText& Label,
        float InitialValue,
        TFunction<void(float)> OnValueChanged
    )
    {
        return SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f)
            [
                SNew(STextBlock)
                .Text(Label)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f)
            [
                SNew(SSpinBox<float>)

                .Value(InitialValue)

                .Delta(0.1f)

                .EnableWheel(false)

                .MinDesiredWidth(100.0f)

                .OnValueChanged_Lambda(
                    [OnValueChanged](float NewValue)
                    {
                        OnValueChanged(NewValue);
                    }
                )
            ];
    }


    // =====================================
    // Apply Import Settings
    // =====================================

    bool ApplyDraftSettings(
        const TSharedPtr<FFBXImportSettingsDraft>& Draft,
        FString& OutMessage
    )
    {
        // ---------------------------------
        // ① 一時データの確認
        // ---------------------------------

        if (!Draft.IsValid())
        {
            OutMessage = TEXT(
                "編集データが存在しません。"
            );

            return false;
        }


        // ---------------------------------
        // ② 適用対象を取得
        // ---------------------------------

        UStaticMesh* TargetMesh =
            Draft->TargetMesh.Get();

        UInterchangeAssetImportData* ImportData =
            Draft->ImportData.Get();

        if (!IsValid(TargetMesh) || !IsValid(ImportData))
        {
            OutMessage = TEXT(
                "適用対象のアセットが無効です。"
            );

            return false;
        }


        // ---------------------------------
        // ③ ImportDataの一致確認
        // ---------------------------------

        if (TargetMesh->AssetImportData.Get() != ImportData)
        {
            OutMessage = TEXT(
                "インポートデータが変更されています。"
                "ツールを開き直してください。"
            );

            return false;
        }


        // ---------------------------------
        // ④ 入力値の検証
        // ---------------------------------

        const bool bTranslationValid =
            FMath::IsFinite(Draft->Translation.X) &&
            FMath::IsFinite(Draft->Translation.Y) &&
            FMath::IsFinite(Draft->Translation.Z);

        const bool bRotationValid =
            FMath::IsFinite(Draft->Rotation.Roll) &&
            FMath::IsFinite(Draft->Rotation.Pitch) &&
            FMath::IsFinite(Draft->Rotation.Yaw);

        const bool bScaleValid =
            FMath::IsFinite(Draft->UniformScale) &&
            !FMath::IsNearlyZero(Draft->UniformScale);

        if (!bTranslationValid || !bRotationValid || !bScaleValid)
        {
            OutMessage = TEXT(
                "入力値が不正です。"
                "数値を確認してください。"
                "Uniform Scaleには0を指定できません。"
            );

            return false;
        }


        // ---------------------------------
        // ⑤ Pipelineを取得
        // ---------------------------------

        TArray<UObject*> Pipelines =
            ImportData->GetPipelines();

        UInterchangeGenericAssetsPipeline* TargetPipeline =
            nullptr;


        // ---------------------------------
        // ⑥ Generic Assets Pipelineを検索
        // ---------------------------------

        for (UObject* PipelineObject : Pipelines)
        {
            if (PipelineObject == nullptr)
            {
                continue;
            }

            UInterchangeGenericAssetsPipeline* GenericPipeline =
                Cast<UInterchangeGenericAssetsPipeline>(
                    PipelineObject
                );

            if (GenericPipeline != nullptr)
            {
                TargetPipeline = GenericPipeline;
                break;
            }
        }


        if (TargetPipeline == nullptr)
        {
            OutMessage = TEXT(
                "Generic Assets Pipelineが見つかりません。"
            );

            return false;
        }


        // ---------------------------------
        // ⑦ Undo用トランザクション
        // ---------------------------------

        FScopedTransaction Transaction(
            FText::FromString(
                TEXT("FBX Reimport Assistant: Apply Import Settings")
            ),
            true
        );
        
        // Undo対象のTransactionalフラグを設定
        TargetMesh->SetFlags(RF_Transactional);
        ImportData->SetFlags(RF_Transactional);
        TargetPipeline->SetFlags(RF_Transactional);
        
        // 変更前の状態をトランザクションに記録
        const bool bMeshModified = TargetMesh->Modify();
        
        const bool bImportDataModified = ImportData->Modify();
        
        const bool bPipelineModified = TargetPipeline->Modify();
        
        // Undo対象への登録確認
        if (!bMeshModified || !bImportDataModified ||!bPipelineModified)
        {
            Transaction.Cancel();
            
            OutMessage = FString::Printf(
                TEXT(
                    "Undo対象の登録に失敗しました。\n"
                    "Mesh: %s\n"
                    "ImportData: %s\n"
                    "Pipeline: %s"),
                    
                    bMeshModified ? TEXT("OK") : TEXT("NG"),
                    bImportDataModified ? TEXT("OK") : TEXT("NG"),
                    bPipelineModified ? TEXT("OK") : TEXT("NG")
                    );
            
            return false;
        }

        // ---------------------------------
        // ⑧ Pipelineの設定値を変更
        // ---------------------------------

        TargetPipeline->ImportOffsetTranslation =
            Draft->Translation;

        TargetPipeline->ImportOffsetRotation =
            Draft->Rotation;

        TargetPipeline->ImportOffsetUniformScale =
            Draft->UniformScale;


        // ---------------------------------
        // ⑨ PipelineをImportDataに再設定
        // ---------------------------------

        ImportData->SetPipelines(Pipelines);


        // ---------------------------------
        // ⑩ アセットを変更済みにする
        // ---------------------------------

        TargetMesh->MarkPackageDirty();


        // ---------------------------------
        // ⑪ 結果を返す
        // ---------------------------------

        OutMessage = FString::Printf(
            TEXT(
                "設定を適用しました。\n"
                "アセット: %s\n\n"
                "Translation: %.2f / %.2f / %.2f\n"
                "Rotation: %.2f / %.2f / %.2f\n"
                "Uniform Scale: %.2f\n\n"
                "アセットを保存してください。"
                "まだ再インポートは実行していません。"
            ),

            *Draft->AssetName,

            Draft->Translation.X,
            Draft->Translation.Y,
            Draft->Translation.Z,

            Draft->Rotation.Roll,
            Draft->Rotation.Pitch,
            Draft->Rotation.Yaw,

            Draft->UniformScale
        );

        return true;
    }


    // =====================================
    // Open Main Window
    // =====================================

    void OpenWindow()
    {
        // ---------------------------------
        // ① Content Browserから選択アセットを取得
        // ---------------------------------

        FContentBrowserModule& ContentBrowserModule =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(
                TEXT("ContentBrowser")
            );

        TArray<FAssetData> SelectedAssets;

        ContentBrowserModule.Get().GetSelectedAssets(
            SelectedAssets
        );


        // ---------------------------------
        // ② アセット情報
        // ---------------------------------

        int32 StaticMeshCount = 0;

        int32 OtherAssetCount = 0;

        FString DiagnosticText;

        TSharedPtr<FFBXImportSettingsDraft> Draft;

        TSharedPtr<FString> ApplyStatus =
            MakeShared<FString>(
                TEXT("まだ設定は適用されていません。")
            );


        for (const FAssetData& AssetData : SelectedAssets)
        {
            UObject* Asset = AssetData.GetAsset();

            UStaticMesh* StaticMesh =
                Cast<UStaticMesh>(Asset);

            if (StaticMesh == nullptr)
            {
                ++OtherAssetCount;
                continue;
            }

            ++StaticMeshCount;

            // 複数選択時は今回の学習では処理しない
            if (StaticMeshCount > 1)
            {
                continue;
            }


            // ---------------------------------
            // ③ ImportDataを取得
            // ---------------------------------

            UAssetImportData* ImportData =
                StaticMesh->AssetImportData.Get();

            if (ImportData == nullptr)
            {
                DiagnosticText = TEXT(
                    "インポートデータが存在しません。"
                );

                continue;
            }


            // ---------------------------------
            // ④ Interchange Import Dataを取得
            // ---------------------------------

            UInterchangeAssetImportData* InterchangeImportData =
                Cast<UInterchangeAssetImportData>(
                    ImportData
                );

            if (InterchangeImportData == nullptr)
            {
                DiagnosticText = TEXT(
                    "Interchange形式ではありません。"
                );

                continue;
            }


            // ---------------------------------
            // ⑤ Pipelineを取得
            // ---------------------------------

            TArray<UObject*> Pipelines =
                InterchangeImportData->GetPipelines();

            bool bFoundPipeline = false;


            for (UObject* PipelineObject : Pipelines)
            {
                if (PipelineObject == nullptr)
                {
                    continue;
                }

                UInterchangeGenericAssetsPipeline* GenericPipeline =
                    Cast<UInterchangeGenericAssetsPipeline>(
                        PipelineObject
                    );

                if (GenericPipeline == nullptr)
                {
                    continue;
                }


                // ---------------------------------
                // ⑥ 一時データを作成
                // ---------------------------------

                Draft =
                    MakeShared<FFBXImportSettingsDraft>();

                Draft->AssetName =
                    StaticMesh->GetName();

                Draft->SourceFilePath =
                    ImportData->GetFirstFilename();

                Draft->Translation =
                    GenericPipeline->ImportOffsetTranslation;

                Draft->Rotation =
                    GenericPipeline->ImportOffsetRotation;

                Draft->UniformScale =
                    GenericPipeline->ImportOffsetUniformScale;

                Draft->TargetMesh = StaticMesh;

                Draft->ImportData = InterchangeImportData;

                bFoundPipeline = true;

                break;
            }


            if (!bFoundPipeline)
            {
                DiagnosticText = TEXT(
                    "Generic Assets Pipelineが見つかりません。"
                );
            }
        }


        // ---------------------------------
        // ⑦ UIレイアウトを作成
        // ---------------------------------

        TSharedRef<SVerticalBox> Content =
            SNew(SVerticalBox);


        // ---------------------------------
        // ⑧ 選択状態を確認
        // ---------------------------------

        if (SelectedAssets.Num() == 0)
        {
            Content->AddSlot()
            .AutoHeight()
            .Padding(10.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        TEXT("コンテンツブラウザでStatic Meshを選択してください。")
                    )
                )
            ];
        }
        else if (StaticMeshCount == 0)
        {
            Content->AddSlot()
            .AutoHeight()
            .Padding(10.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        TEXT("Static Meshが選択されていません。")
                    )
                )
            ];
        }
        else if (StaticMeshCount > 1)
        {
            Content->AddSlot()
            .AutoHeight()
            .Padding(10.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        TEXT("今回の学習ではStatic Meshを1つだけ選択してください。")
                    )
                )
            ];
        }
        else if (!Draft.IsValid())
        {
            Content->AddSlot()
            .AutoHeight()
            .Padding(10.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        DiagnosticText.IsEmpty()
                        ? TEXT("インポート設定を取得できません。")
                        : DiagnosticText
                    )
                )
            ];
        }
        else
        {
            // ---------------------------------
            // ⑨ 対象アセット
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        FString::Printf(
                            TEXT("対象アセット: %s"),
                            *Draft->AssetName
                        )
                    )
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        FString::Printf(
                            TEXT("FBXソース: %s"),
                            *Draft->SourceFilePath
                        )
                    )
                )
                .AutoWrapText(true)
            ];


            // ---------------------------------
            // ⑩ Translation Offset
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        TEXT("Translation Offset")
                    )
                )
            ];


            TSharedRef<SHorizontalBox> TranslationRow =
                SNew(SHorizontalBox);


            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("X")),
                    Draft->Translation.X,

                    [Draft](float NewValue)
                    {
                        Draft->Translation.X = NewValue;
                    }
                )
            ];


            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("Y")),
                    Draft->Translation.Y,

                    [Draft](float NewValue)
                    {
                        Draft->Translation.Y = NewValue;
                    }
                )
            ];


            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("Z")),
                    Draft->Translation.Z,

                    [Draft](float NewValue)
                    {
                        Draft->Translation.Z = NewValue;
                    }
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            [
                TranslationRow
            ];


            // ---------------------------------
            // ⑪ Rotation Offset
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        TEXT("Rotation Offset")
                    )
                )
            ];


            TSharedRef<SHorizontalBox> RotationRow =
                SNew(SHorizontalBox);


            RotationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("Roll")),
                    Draft->Rotation.Roll,

                    [Draft](float NewValue)
                    {
                        Draft->Rotation.Roll = NewValue;
                    }
                )
            ];


            RotationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("Pitch")),
                    Draft->Rotation.Pitch,

                    [Draft](float NewValue)
                    {
                        Draft->Rotation.Pitch = NewValue;
                    }
                )
            ];


            RotationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("Yaw")),
                    Draft->Rotation.Yaw,

                    [Draft](float NewValue)
                    {
                        Draft->Rotation.Yaw = NewValue;
                    }
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            [
                RotationRow
            ];


            // ---------------------------------
            // ⑫ Uniform Scale
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        TEXT("Uniform Scale")
                    )
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("Scale")),
                    Draft->UniformScale,

                    [Draft](float NewValue)
                    {
                        Draft->UniformScale = NewValue;
                    }
                )
            ];


            // ---------------------------------
            // ⑬ 一時データのプレビュー
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text_Lambda(
                    [Draft]()
                    {
                        return FText::FromString(
                            FString::Printf(
                                TEXT("現在の一時データ\n")
                                TEXT("Translation: %.2f / %.2f / %.2f\n")
                                TEXT("Rotation: %.2f / %.2f / %.2f\n")
                                TEXT("Uniform Scale: %.2f"),

                                Draft->Translation.X,
                                Draft->Translation.Y,
                                Draft->Translation.Z,

                                Draft->Rotation.Roll,
                                Draft->Rotation.Pitch,
                                Draft->Rotation.Yaw,

                                Draft->UniformScale
                            )
                        );
                    }
                )
                .AutoWrapText(true)
            ];


            // ---------------------------------
            // ⑭ 設定適用ボタン
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(SButton)

                .Text(
                    FText::FromString(
                        TEXT("設定を適用")
                    )
                )

                .OnClicked_Lambda(
                    [this, Draft, ApplyStatus]()
                    {
                        FString ResultMessage;

                        ApplyDraftSettings(
                            Draft,
                            ResultMessage
                        );

                        *ApplyStatus = ResultMessage;

                        return FReply::Handled();
                    }
                )
            ];


            // ---------------------------------
            // ⑮ 適用結果
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)

                .Text_Lambda(
                    [ApplyStatus]()
                    {
                        return FText::FromString(
                            *ApplyStatus
                        );
                    }
                )

                .AutoWrapText(true)
            ];
        }


        // ---------------------------------
        // ⑯ 注意書き
        // ---------------------------------

        Content->AddSlot()
        .AutoHeight()
        .Padding(10.0f)
        [
            SNew(STextBlock)
            .Text(
                FText::FromString(
                    TEXT(
                        "注意: 設定を適用してもメッシュ形状は変わりません。"
                        "実際の反映にはFBXの再インポートが必要です。"
                    )
                )
            )
            .AutoWrapText(true)
        ];


        // ---------------------------------
        // ⑰ ウィンドウ生成
        // ---------------------------------

        TSharedRef<SWindow> Window =
            SNew(SWindow)
            .Title(
                FText::FromString(
                    TEXT("FBX Reimport Assistant")
                )
            )
            .ClientSize(
                FVector2D(650.0f, 700.0f)
            );


        Window->SetContent(
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                Content
            ]
        );


        FSlateApplication::Get().AddWindow(Window);
    }

};


// =====================================
// Implement Module
// =====================================

IMPLEMENT_MODULE(
    FFBXReimportAssistantModule,
    FBXReimportAssistant
)
