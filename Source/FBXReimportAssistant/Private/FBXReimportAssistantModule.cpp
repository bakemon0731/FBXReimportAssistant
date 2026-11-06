
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
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"

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
                "FBX Reimport Assistant"
            ),

            FText::FromString(
                "Open FBX Reimport Assistant Window"
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

            // Label
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f)
            [
                SNew(STextBlock)
                .Text(Label)
            ]

            // Numeric Input
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
        // ② Static Meshと設定データを取得
        // ---------------------------------

        int32 StaticMeshCount = 0;

        int32 OtherAssetCount = 0;

        FString DiagnosticText;

        TSharedPtr<FFBXImportSettingsDraft> Draft;


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

            // 今回はStatic Meshを1つだけ編集対象にする
            if (StaticMeshCount > 1)
            {
                continue;
            }


            // ---------------------------------
            // ③ Import Dataを取得
            // ---------------------------------

            UAssetImportData* ImportData =
                StaticMesh->AssetImportData.Get();

            if (ImportData == nullptr)
            {
                DiagnosticText =
                    TEXT("インポートデータが存在しません。");

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
                DiagnosticText =
                    FString::Printf(
                        TEXT("Interchange形式ではありません。\n")
                        TEXT("実際の型: %s"),

                        *ImportData->GetClass()->GetName()
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
                // ⑥ 一時データに設定値をコピー
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

                bFoundPipeline = true;

                break;
            }


            if (!bFoundPipeline)
            {
                DiagnosticText =
                    FString::Printf(
                        TEXT("Generic Assets Pipelineが見つかりません。\n")
                        TEXT("保存Pipeline数: %d"),

                        Pipelines.Num()
                    );
            }
        }


        // ---------------------------------
        // ⑦ ウィンドウのルートレイアウトを作成
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
                        "コンテンツブラウザでStatic Meshを選択してください。"
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
                        "Static Meshが選択されていません。"
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
                        "今回の学習ではStatic Meshを1つだけ選択してください。"
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
            // ⑨ 対象アセット情報
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
                        "Translation Offset"
                    )
                )
            ];


            TSharedRef<SHorizontalBox> TranslationRow =
                SNew(SHorizontalBox);


            // X
            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("X"),
                    Draft->Translation.X,

                    [Draft](float NewValue)
                    {
                        Draft->Translation.X = NewValue;
                    }
                )
            ];


            // Y
            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("Y"),
                    Draft->Translation.Y,

                    [Draft](float NewValue)
                    {
                        Draft->Translation.Y = NewValue;
                    }
                )
            ];


            // Z
            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("Z"),
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
                        "Rotation Offset"
                    )
                )
            ];


            TSharedRef<SHorizontalBox> RotationRow =
                SNew(SHorizontalBox);


            // Roll
            RotationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("Roll"),
                    Draft->Rotation.Roll,

                    [Draft](float NewValue)
                    {
                        Draft->Rotation.Roll = NewValue;
                    }
                )
            ];


            // Pitch
            RotationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("Pitch"),
                    Draft->Rotation.Pitch,

                    [Draft](float NewValue)
                    {
                        Draft->Rotation.Pitch = NewValue;
                    }
                )
            ];


            // Yaw
            RotationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("Yaw"),
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
                        "Uniform Scale"
                    )
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString("Scale"),
                    Draft->UniformScale,

                    [Draft](float NewValue)
                    {
                        Draft->UniformScale = NewValue;
                    }
                )
            ];


            // ---------------------------------
            // ⑬ 入力値のプレビュー
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
        }


        // ---------------------------------
        // ⑭ 注意書き
        // ---------------------------------

        Content->AddSlot()
        .AutoHeight()
        .Padding(10.0f)
        [
            SNew(STextBlock)
            .Text(
                FText::FromString(
                    TEXT("注意: 現在の入力値は一時データです。")
                    TEXT("元のアセット設定にはまだ反映されません。")
                )
            )
            .AutoWrapText(true)
        ];


        // ---------------------------------
        // ⑮ Slate Window
        // ---------------------------------

        TSharedRef<SWindow> Window =
            SNew(SWindow)
            .Title(
                FText::FromString(
                    "FBX Reimport Assistant"
                )
            )
            .ClientSize(
                FVector2D(650.0f, 650.0f)
            );


        // ScrollBoxにルートレイアウトを設定
        Window->SetContent(
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                Content
            ]
        );


        // ウィンドウ表示
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
