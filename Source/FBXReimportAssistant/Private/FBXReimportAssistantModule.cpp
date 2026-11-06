
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
        // ② Static Meshを判定・取得
        // ---------------------------------

        int32 StaticMeshCount = 0;
        int32 OtherAssetCount = 0;

        FString MeshDetails;

        for (const FAssetData& AssetData : SelectedAssets)
        {
            // アセット本体を取得
            UObject* Asset = AssetData.GetAsset();

            // Static Meshかどうか判定
            UStaticMesh* StaticMesh =
                Cast<UStaticMesh>(Asset);

            if (StaticMesh == nullptr)
            {
                ++OtherAssetCount;
                continue;
            }

            // Static Meshとして取得成功
            ++StaticMeshCount;

            MeshDetails += FString::Printf(
                TEXT("========================================\n")
                TEXT("アセット名: %s\n"),
                *StaticMesh->GetName()
            );


            // ---------------------------------
            // ③ インポートデータを取得
            // ---------------------------------

            UAssetImportData* ImportData =
                StaticMesh->AssetImportData.Get();

            if (ImportData == nullptr)
            {
                MeshDetails += TEXT(
                    "インポートデータ: 存在しません\n\n"
                );

                continue;
            }


            // インポートデータの型を表示
            FString ImportDataClassName =
                ImportData->GetClass()->GetName();

            MeshDetails += FString::Printf(
                TEXT("インポートデータ型: %s\n"),
                *ImportDataClassName
            );


            // ---------------------------------
            // ④ ソースファイルパスを取得
            // ---------------------------------

            FString SourceFilePath =
                ImportData->GetFirstFilename();

            if (SourceFilePath.IsEmpty())
            {
                MeshDetails += TEXT(
                    "ソースファイル: 登録されていません\n"
                );
            }
            else
            {
                MeshDetails += FString::Printf(
                    TEXT("ソースファイル: %s\n"),
                    *SourceFilePath
                );
            }


            // ---------------------------------
            // ⑤ Interchange Import Dataを取得
            // ---------------------------------

            UInterchangeAssetImportData* InterchangeImportData =
                Cast<UInterchangeAssetImportData>(ImportData);

            if (InterchangeImportData == nullptr)
            {
                MeshDetails += TEXT(
                    "Interchange Import Dataではありません。\n\n"
                );

                continue;
            }


            // ---------------------------------
            // ⑥ 保存されているPipelineを取得
            // ---------------------------------

            TArray<UObject*> Pipelines =
                InterchangeImportData->GetPipelines();

            MeshDetails += FString::Printf(
                TEXT("保存Pipeline数: %d\n"),
                Pipelines.Num()
            );

            bool bFoundGenericAssetsPipeline = false;


            // ---------------------------------
            // ⑦ Pipelineを順番に確認
            // ---------------------------------

            for (UObject* PipelineObject : Pipelines)
            {
                if (PipelineObject == nullptr)
                {
                    continue;
                }

                // Pipelineのクラス名を表示
                MeshDetails += FString::Printf(
                    TEXT("Pipeline型: %s\n"),
                    *PipelineObject->GetClass()->GetName()
                );


                // Generic Assets Pipelineに変換
                UInterchangeGenericAssetsPipeline* GenericPipeline =
                    Cast<UInterchangeGenericAssetsPipeline>(
                        PipelineObject
                    );

                if (GenericPipeline == nullptr)
                {
                    continue;
                }

                bFoundGenericAssetsPipeline = true;


                // ---------------------------------
                // ⑧ Interchangeの設定値を取得
                // ---------------------------------

                FVector Translation =
                    GenericPipeline->ImportOffsetTranslation;

                FRotator Rotation =
                    GenericPipeline->ImportOffsetRotation;

                float UniformScale =
                    GenericPipeline->ImportOffsetUniformScale;


                // ---------------------------------
                // ⑨ 設定値を文字列に変換
                // ---------------------------------

                MeshDetails += FString::Printf(
                    TEXT("\n--- インポート設定 ---\n")
                    TEXT("Translation: X=%.2f Y=%.2f Z=%.2f\n")
                    TEXT("Rotation: Roll=%.2f Pitch=%.2f Yaw=%.2f\n")
                    TEXT("Uniform Scale: %.2f\n"),

                    Translation.X,
                    Translation.Y,
                    Translation.Z,

                    Rotation.Roll,
                    Rotation.Pitch,
                    Rotation.Yaw,

                    UniformScale
                );
            }


            // ---------------------------------
            // ⑩ Generic Assets Pipelineがない場合
            // ---------------------------------

            if (!bFoundGenericAssetsPipeline)
            {
                MeshDetails += TEXT(
                    "Generic Assets Pipelineを取得できません。\n"
                );
            }

            MeshDetails += TEXT("\n");
        }


        // ---------------------------------
        // ⑪ 表示内容を作成
        // ---------------------------------

        FString WindowText = FString::Printf(
            TEXT("選択アセット数: %d\n")
            TEXT("Static Mesh数: %d\n")
            TEXT("その他のアセット数: %d\n\n"),

            SelectedAssets.Num(),
            StaticMeshCount,
            OtherAssetCount
        );

        if (SelectedAssets.Num() == 0)
        {
            WindowText += TEXT(
                "コンテンツブラウザでアセットを選択してください。"
            );
        }
        else if (StaticMeshCount == 0)
        {
            WindowText += TEXT(
                "Static Meshが選択されていません。"
            );
        }
        else
        {
            WindowText += MeshDetails;
        }


        // ---------------------------------
        // ⑫ Slateウィンドウを生成
        // ---------------------------------

        TSharedRef<SWindow> Window =
            SNew(SWindow)
            .Title(
                FText::FromString(
                    "FBX Reimport Assistant"
                )
            )
            .ClientSize(
                FVector2D(750.0f, 500.0f)
            );

        Window->SetContent(
            SNew(STextBlock)
            .Text(
                FText::FromString(WindowText)
            )
            .AutoWrapText(true)
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
