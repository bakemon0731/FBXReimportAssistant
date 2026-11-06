
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
#include "InterchangeManager.h"

// =====================================
// File System
// =====================================
#include "Misc/Paths.h"

// =====================================
// Dialog
// =====================================
#include "Misc/MessageDialog.h"

// =====================================
// Async
// =====================================
#include "Async/Async.h"


// =====================================
// Import Settings Draft
// UI編集用の一時データ
// =====================================

struct FFBXImportSettingsDraft
{
    // アセット名
    FString AssetName;

    // FBXソースパス
    FString SourceFilePath;

    // インポート設定
    FVector Translation = FVector::ZeroVector;

    FRotator Rotation = FRotator::ZeroRotator;

    float UniformScale = 1.0f;

    // 対象アセット
    TWeakObjectPtr<UStaticMesh> TargetMesh;

    // Interchange Import Data
    TWeakObjectPtr<UInterchangeAssetImportData> ImportData;

    // 設定適用状態
    bool bSettingsApplied = true;

    // 再インポート実行状態
    bool bReimportInProgress = false;
};


// =====================================
// FBX Reimport Assistant Module
// =====================================

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
    // Validate and Create Draft
    // 再インポート可能なアセットか確認
    // =====================================

    bool TryCreateDraft(
        UStaticMesh* StaticMesh,
        TSharedPtr<FFBXImportSettingsDraft>& OutDraft,
        FString& OutError
    )
    {
        OutDraft.Reset();

        // ---------------------------------
        // ① Static Meshの有効性を確認
        // ---------------------------------

        if (!IsValid(StaticMesh))
        {
            OutError = TEXT(
                "Static Meshが無効です。"
            );

            return false;
        }


        // ---------------------------------
        // ② Interchange Managerを取得
        // ---------------------------------

        UInterchangeManager& Manager =
            UInterchangeManager::GetInterchangeManager();


        // ---------------------------------
        // ③ インポート中か確認
        // ---------------------------------

        if (Manager.IsObjectBeingImported(StaticMesh))
        {
            OutError = FString::Printf(
                TEXT(
                    "アセットは現在インポート中です。\n"
                    "アセット: %s"
                ),

                *StaticMesh->GetName()
            );

            return false;
        }


        // ---------------------------------
        // ④ 再インポート可能か確認
        // ---------------------------------

        TArray<FString> SourceFiles;

        const bool bCanReimport =
            Manager.CanReimport(
                StaticMesh,
                SourceFiles
            );

        if (!bCanReimport)
        {
            OutError = FString::Printf(
                TEXT(
                    "このメッシュはFBX再インポートの対象外です。\n\n"
                    "アセット: %s\n\n"
                    "原因:\n"
                    "- FBXのインポート情報が存在しない\n"
                    "- Interchangeが再インポートに対応していない\n"
                    "- Modeling Modeなどで生成されたアセット"
                ),

                *StaticMesh->GetName()
            );

            return false;
        }


        // ---------------------------------
        // ⑤ ソースファイル数を確認
        // ---------------------------------

        if (SourceFiles.Num() != 1)
        {
            OutError = FString::Printf(
                TEXT(
                    "このツールではFBXソースが1つのアセットだけを扱います。\n\n"
                    "アセット: %s\n"
                    "ソースファイル数: %d"
                ),

                *StaticMesh->GetName(),
                SourceFiles.Num()
            );

            return false;
        }


        // ---------------------------------
        // ⑥ FBXファイルのパスを確認
        // ---------------------------------

        const FString SourceFilePath =
            SourceFiles[0];

        const FString Extension =
            FPaths::GetExtension(SourceFilePath);

        if (!Extension.Equals(
            TEXT("fbx"),
            ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT(
                    "このツールはFBXファイルのみ対応しています。\n\n"
                    "検出されたファイル:\n%s"
                ),

                *SourceFilePath
            );

            return false;
        }


        // ---------------------------------
        // ⑦ ファイルの存在確認
        // ---------------------------------

        if (!FPaths::FileExists(SourceFilePath))
        {
            OutError = FString::Printf(
                TEXT(
                    "元のFBXファイルが見つかりません。\n\n"
                    "%s"
                ),

                *SourceFilePath
            );

            return false;
        }


        // ---------------------------------
        // ⑧ Import Dataの取得
        // ---------------------------------

        UAssetImportData* BaseImportData =
            StaticMesh->AssetImportData.Get();

        if (!IsValid(BaseImportData))
        {
            OutError = TEXT(
                "Import Dataが存在しません。"
            );

            return false;
        }


        // ---------------------------------
        // ⑨ Interchange Import Dataの型を確認
        // ---------------------------------

        UInterchangeAssetImportData* InterchangeImportData =
            Cast<UInterchangeAssetImportData>(
                BaseImportData
            );

        if (!IsValid(InterchangeImportData))
        {
            OutError = FString::Printf(
                TEXT(
                    "Interchange Import Dataではありません。\n\n"
                    "実際の型: %s"
                ),

                *BaseImportData->GetClass()->GetName()
            );

            return false;
        }


        // ---------------------------------
        // ⑩ Pipelineを取得
        // ---------------------------------

        TArray<UObject*> Pipelines =
            InterchangeImportData->GetPipelines();

        if (Pipelines.IsEmpty())
        {
            OutError = TEXT(
                "Interchange Pipelineが存在しません。"
            );

            return false;
        }


        // ---------------------------------
        // ⑪ Generic Assets Pipelineを検索
        // ---------------------------------

        UInterchangeGenericAssetsPipeline* GenericPipeline =
            nullptr;

        for (UObject* PipelineObject : Pipelines)
        {
            if (!IsValid(PipelineObject))
            {
                continue;
            }

            GenericPipeline =
                Cast<UInterchangeGenericAssetsPipeline>(
                    PipelineObject
                );

            if (GenericPipeline != nullptr)
            {
                break;
            }
        }


        if (!IsValid(GenericPipeline))
        {
            OutError = FString::Printf(
                TEXT(
                    "Generic Assets Pipelineが見つかりません。\n"
                    "アセット: %s\n"
                    "Pipeline数: %d"
                ),

                *StaticMesh->GetName(),
                Pipelines.Num()
            );

            return false;
        }


        // ---------------------------------
        // ⑫ Draftを作成
        // ---------------------------------

        OutDraft =
            MakeShared<FFBXImportSettingsDraft>();

        OutDraft->AssetName =
            StaticMesh->GetName();

        OutDraft->SourceFilePath =
            SourceFilePath;

        OutDraft->Translation =
            GenericPipeline->ImportOffsetTranslation;

        OutDraft->Rotation =
            GenericPipeline->ImportOffsetRotation;

        OutDraft->UniformScale =
            GenericPipeline->ImportOffsetUniformScale;

        OutDraft->TargetMesh =
            StaticMesh;

        OutDraft->ImportData =
            InterchangeImportData;

        // 取得直後の値はPipelineと一致しているため適用済み
        OutDraft->bSettingsApplied = true;

        OutDraft->bReimportInProgress = false;


        return true;
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
        // ① Draftの確認
        // ---------------------------------

        if (!Draft.IsValid())
        {
            OutMessage = TEXT(
                "編集データが存在しません。"
            );

            return false;
        }


        // ---------------------------------
        // ② 対象アセットを取得
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
                "インポートデータが変更されています。\n"
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
                "入力値が不正です。\n"
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

        for (UObject* PipelineObject : Pipelines)
        {
            if (!IsValid(PipelineObject))
            {
                continue;
            }

            TargetPipeline =
                Cast<UInterchangeGenericAssetsPipeline>(
                    PipelineObject
                );

            if (TargetPipeline != nullptr)
            {
                break;
            }
        }


        if (!IsValid(TargetPipeline))
        {
            OutMessage = TEXT(
                "Generic Assets Pipelineが見つかりません。"
            );

            return false;
        }


        // ---------------------------------
        // ⑥ Undo用トランザクション
        // ---------------------------------

        FScopedTransaction Transaction(
            FText::FromString(
                TEXT("FBX Reimport Assistant: Apply Import Settings")
            ),
            true
        );

        TargetMesh->SetFlags(RF_Transactional);

        ImportData->SetFlags(RF_Transactional);

        TargetPipeline->SetFlags(RF_Transactional);


        const bool bMeshModified =
            TargetMesh->Modify();

        const bool bImportDataModified =
            ImportData->Modify();

        const bool bPipelineModified =
            TargetPipeline->Modify();


        if (!bMeshModified ||
            !bImportDataModified ||
            !bPipelineModified)
        {
            Transaction.Cancel();

            OutMessage = FString::Printf(
                TEXT(
                    "Undo対象の登録に失敗しました。\n"
                    "Mesh: %s\n"
                    "ImportData: %s\n"
                    "Pipeline: %s"
                ),

                bMeshModified ? TEXT("OK") : TEXT("NG"),
                bImportDataModified ? TEXT("OK") : TEXT("NG"),
                bPipelineModified ? TEXT("OK") : TEXT("NG")
            );

            return false;
        }


        // ---------------------------------
        // ⑦ Pipelineの設定値を変更
        // ---------------------------------

        TargetPipeline->ImportOffsetTranslation =
            Draft->Translation;

        TargetPipeline->ImportOffsetRotation =
            Draft->Rotation;

        TargetPipeline->ImportOffsetUniformScale =
            Draft->UniformScale;


        // ---------------------------------
        // ⑧ PipelineをImportDataに再設定
        // ---------------------------------

        ImportData->SetPipelines(Pipelines);


        // ---------------------------------
        // ⑨ アセットを変更済みにする
        // ---------------------------------

        TargetMesh->MarkPackageDirty();


        // ---------------------------------
        // ⑩ 適用状態を更新
        // ---------------------------------

        Draft->bSettingsApplied = true;


        OutMessage = FString::Printf(
            TEXT(
                "設定を適用しました。\n"
                "アセット: %s\n\n"
                "Translation: %.2f / %.2f / %.2f\n"
                "Rotation: %.2f / %.2f / %.2f\n"
                "Uniform Scale: %.2f\n\n"
                "アセットを保存してください。\n"
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
    // Start FBX Reimport
    // =====================================

    void StartReimport(
        const TSharedPtr<FFBXImportSettingsDraft>& Draft,
        const TSharedPtr<FString>& StatusText
    )
    {
        // ---------------------------------
        // ① Draftの確認
        // ---------------------------------

        if (!Draft.IsValid())
        {
            *StatusText = TEXT(
                "編集データが存在しません。"
            );

            return;
        }


        // ---------------------------------
        // ② 設定適用状態の確認
        // ---------------------------------

        if (!Draft->bSettingsApplied)
        {
            *StatusText = TEXT(
                "設定が適用されていません。\n"
                "先に「設定を適用」を実行してください。"
            );

            return;
        }


        // ---------------------------------
        // ③ 二重実行の防止
        // ---------------------------------

        if (Draft->bReimportInProgress)
        {
            *StatusText = TEXT(
                "すでに再インポートを実行中です。"
            );

            return;
        }


        // ---------------------------------
        // ④ 対象アセットの取得
        // ---------------------------------

        UStaticMesh* TargetMesh =
            Draft->TargetMesh.Get();

        UInterchangeAssetImportData* ImportData =
            Draft->ImportData.Get();

        if (!IsValid(TargetMesh) || !IsValid(ImportData))
        {
            *StatusText = TEXT(
                "対象アセットが無効です。\n"
                "ツールを開き直してください。"
            );

            return;
        }


        // ---------------------------------
        // ⑤ ImportDataの一致確認
        // ---------------------------------

        if (TargetMesh->AssetImportData.Get() != ImportData)
        {
            *StatusText = TEXT(
                "Import Dataが変更されています。\n"
                "ツールを開き直してください。"
            );

            return;
        }


        // ---------------------------------
        // ⑥ Interchange Manager
        // ---------------------------------

        UInterchangeManager& Manager =
            UInterchangeManager::GetInterchangeManager();


        if (Manager.IsObjectBeingImported(TargetMesh))
        {
            *StatusText = TEXT(
                "このアセットは現在インポート中です。"
            );

            return;
        }


        // ---------------------------------
        // ⑦ 再インポート可能か再確認
        // ---------------------------------

        TArray<FString> VerifiedSourceFiles;

        const bool bCanReimport =
            Manager.CanReimport(
                TargetMesh,
                VerifiedSourceFiles
            );

        if (!bCanReimport)
        {
            *StatusText = TEXT(
                "このアセットは再インポートできません。"
            );

            return;
        }


        if (VerifiedSourceFiles.Num() != 1)
        {
            *StatusText = FString::Printf(
                TEXT(
                    "ソースファイル数が対象外です。\n"
                    "検出数: %d"
                ),

                VerifiedSourceFiles.Num()
            );

            return;
        }


        // ---------------------------------
        // ⑧ FBXパスの確認
        // ---------------------------------

        const FString SourceFilePath =
            VerifiedSourceFiles[0];

        if (!FPaths::GetExtension(SourceFilePath).Equals(
            TEXT("fbx"),
            ESearchCase::IgnoreCase))
        {
            *StatusText = TEXT(
                "FBXファイルではありません。"
            );

            return;
        }


        if (!FPaths::FileExists(SourceFilePath))
        {
            *StatusText = FString::Printf(
                TEXT(
                    "FBXファイルが存在しません。\n%s"
                ),

                *SourceFilePath
            );

            return;
        }


        // ---------------------------------
        // ⑨ 実行確認
        // ---------------------------------

        const EAppReturnType::Type Response =
            FMessageDialog::Open(
                EAppMsgType::YesNo,

                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "以下のアセットを再インポートします。\n\n"
                            "アセット: %s\n"
                            "FBX: %s\n\n"
                            "既存メッシュデータが更新されます。\n"
                            "実行しますか？"
                        ),

                        *Draft->AssetName,
                        *SourceFilePath
                    )
                )
            );


        if (Response != EAppReturnType::Yes)
        {
            *StatusText = TEXT(
                "再インポートをキャンセルしました。"
            );

            return;
        }


        // ---------------------------------
        // ⑩ Import Parameters
        // ---------------------------------

        FImportAssetParameters Parameters;

        Parameters.bIsAutomated = true;

        Parameters.bReplaceExisting = true;

        Parameters.ReimportSourceIndex = 0;


        // ---------------------------------
        // ⑪ 再インポート開始
        // ---------------------------------

        Draft->bReimportInProgress = true;

        *StatusText = TEXT(
            "再インポートを開始しました..."
        );


        UE::Interchange::FAssetImportResultRef ImportResult =
            Manager.ReimportAssetAsync(
                TargetMesh,
                Parameters
            );


        if (!ImportResult->IsValid())
        {
            Draft->bReimportInProgress = false;

            *StatusText = TEXT(
                "再インポートを開始できませんでした。"
            );

            return;
        }


        // ---------------------------------
        // ⑫ 完了通知
        // ---------------------------------

        TWeakPtr<FFBXImportSettingsDraft> WeakDraft =
            Draft;

        TWeakPtr<FString> WeakStatusText =
            StatusText;

        const FString AssetName =
            Draft->AssetName;


        ImportResult->OnDone(
            [
                WeakDraft,
                WeakStatusText,
                AssetName
            ]
            (UE::Interchange::FImportResult& Result)
            {
                const bool bCompleted =
                    Result.GetStatus() ==
                    UE::Interchange::FImportResult::EStatus::Done;

                const int32 ImportedObjectCount =
                    Result.GetImportedObjects().Num();


                // ---------------------------------
                // ⑬ Game Threadで結果を反映
                // ---------------------------------

                AsyncTask(
                    ENamedThreads::GameThread,

                    [
                        WeakDraft,
                        WeakStatusText,
                        AssetName,
                        bCompleted,
                        ImportedObjectCount
                    ]()
                    {
                        TSharedPtr<FFBXImportSettingsDraft> CurrentDraft =
                            WeakDraft.Pin();

                        TSharedPtr<FString> CurrentStatus =
                            WeakStatusText.Pin();


                        if (CurrentDraft.IsValid())
                        {
                            CurrentDraft->bReimportInProgress =
                                false;

                            // 再インポート後の情報を取得し直す
                            CurrentDraft->bSettingsApplied =
                                false;
                        }


                        if (!CurrentStatus.IsValid())
                        {
                            return;
                        }


                        // ---------------------------------
                        // ⑭ 結果を表示
                        // ---------------------------------

                        if (bCompleted && ImportedObjectCount > 0)
                        {
                            *CurrentStatus =
                                FString::Printf(
                                    TEXT(
                                        "再インポート処理が完了しました。\n\n"
                                        "対象アセット: %s\n"
                                        "結果オブジェクト数: %d\n\n"
                                        "メッシュを確認して保存してください。\n"
                                        "最新情報を取得するにはツールを開き直してください。"
                                    ),

                                    *AssetName,
                                    ImportedObjectCount
                                );
                        }
                        else
                        {
                            *CurrentStatus =
                                FString::Printf(
                                    TEXT(
                                        "再インポートが正常に完了しませんでした。\n"
                                        "結果オブジェクト数: %d\n\n"
                                        "Output Logを確認してください。"
                                    ),

                                    ImportedObjectCount
                                );
                        }
                    }
                );
            }
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
        // ② Static Meshを抽出
        // ---------------------------------

        TArray<UStaticMesh*> SelectedStaticMeshes;

        int32 OtherAssetCount = 0;

        for (const FAssetData& AssetData : SelectedAssets)
        {
            UObject* Asset = AssetData.GetAsset();

            if (!IsValid(Asset))
            {
                ++OtherAssetCount;
                continue;
            }

            UStaticMesh* StaticMesh =
                Cast<UStaticMesh>(Asset);

            if (StaticMesh != nullptr)
            {
                SelectedStaticMeshes.Add(StaticMesh);
            }
            else
            {
                ++OtherAssetCount;
            }
        }


        // ---------------------------------
        // ③ Draftの作成と事前検証
        // ---------------------------------

        TSharedPtr<FFBXImportSettingsDraft> Draft;

        FString DiagnosticText;

        if (SelectedAssets.Num() == 0)
        {
            DiagnosticText = TEXT(
                "コンテンツブラウザでStatic Meshを選択してください。"
            );
        }
        else if (SelectedStaticMeshes.IsEmpty())
        {
            DiagnosticText = TEXT(
                "Static Meshが選択されていません。"
            );
        }
        else if (SelectedStaticMeshes.Num() > 1)
        {
            DiagnosticText = TEXT(
                "今回のツールはStatic Meshを1つずつ処理します。\n"
                "複数選択を解除してください。"
            );
        }
        else
        {
            TryCreateDraft(
                SelectedStaticMeshes[0],
                Draft,
                DiagnosticText
            );
        }


        // ---------------------------------
        // ④ ステータス
        // ---------------------------------

        TSharedPtr<FString> ApplyStatus =
            MakeShared<FString>(
                TEXT("まだ設定は適用されていません。")
            );

        TSharedPtr<FString> ReimportStatus =
            MakeShared<FString>(
                TEXT("再インポートはまだ実行されていません。")
            );


        // ---------------------------------
        // ⑤ UIルート
        // ---------------------------------

        TSharedRef<SVerticalBox> Content =
            SNew(SVerticalBox);


        // ---------------------------------
        // ⑥ エラー・対象外の場合
        // ---------------------------------

        if (!Draft.IsValid())
        {
            Content->AddSlot()
            .AutoHeight()
            .Padding(10.0f)
            [
                SNew(STextBlock)
                .Text(
                    FText::FromString(
                        DiagnosticText
                    )
                )
                .AutoWrapText(true)
            ];
        }
        else
        {
            // ---------------------------------
            // ⑦ 対象アセット情報
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


            // ---------------------------------
            // ⑧ Translation
            // ---------------------------------

            TSharedRef<SHorizontalBox> TranslationRow =
                SNew(SHorizontalBox);


            TranslationRow->AddSlot()
            .FillWidth(1.0f)
            .Padding(4.0f)
            [
                MakeNumericField(
                    FText::FromString(TEXT("X")),
                    Draft->Translation.X,

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->Translation.X = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
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

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->Translation.Y = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
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

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->Translation.Z = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
                    }
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                TranslationRow
            ];


            // ---------------------------------
            // ⑨ Rotation
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

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->Rotation.Roll = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
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

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->Rotation.Pitch = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
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

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->Rotation.Yaw = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
                    }
                )
            ];


            Content->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                RotationRow
            ];


            // ---------------------------------
            // ⑩ Uniform Scale
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

                    [Draft, ApplyStatus](float NewValue)
                    {
                        Draft->UniformScale = NewValue;

                        Draft->bSettingsApplied = false;

                        *ApplyStatus = TEXT(
                            "未適用の変更があります。"
                        );
                    }
                )
            ];


            // ---------------------------------
            // ⑪ 一時データのプレビュー
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
                                TEXT(
                                    "現在の一時データ\n"
                                    "Translation: %.2f / %.2f / %.2f\n"
                                    "Rotation: %.2f / %.2f / %.2f\n"
                                    "Uniform Scale: %.2f"
                                ),

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
            // ⑫ 設定適用ボタン
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

                        const bool bSuccess =
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
            // ⑬ 設定適用結果
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


            // ---------------------------------
            // ⑭ FBX再インポートボタン
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(SButton)

                .Text(
                    FText::FromString(
                        TEXT("FBXを再インポート")
                    )
                )

                .IsEnabled_Lambda(
                    [Draft]()
                    {
                        return Draft->bSettingsApplied &&
                               !Draft->bReimportInProgress;
                    }
                )

                .OnClicked_Lambda(
                    [this, Draft, ReimportStatus]()
                    {
                        StartReimport(
                            Draft,
                            ReimportStatus
                        );

                        return FReply::Handled();
                    }
                )
            ];


            // ---------------------------------
            // ⑮ 再インポート結果
            // ---------------------------------

            Content->AddSlot()
            .AutoHeight()
            .Padding(8.0f)
            [
                SNew(STextBlock)

                .Text_Lambda(
                    [ReimportStatus]()
                    {
                        return FText::FromString(
                            *ReimportStatus
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
                        "このツールはFBXソースを持つ"
                        "Interchange対応Static Meshを対象とします。"
                        "\n"
                        "Modeling Modeなどで生成された"
                        "再インポート非対応メッシュは処理しません。"
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
                FVector2D(700.0f, 700.0f)
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
