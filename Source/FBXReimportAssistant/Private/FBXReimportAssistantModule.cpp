
#include "Modules/ModuleManager.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

class FFBXReimportAssistantModule : public IModuleInterface
{
public:

    virtual void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(
                this,
                &FFBXReimportAssistantModule::RegisterMenus
            )
        );
    }

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);

        UToolMenus::UnregisterOwner(this);
    }

private:

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);

        UToolMenu* Menu =
            UToolMenus::Get()->ExtendMenu(
                "LevelEditor.MainMenu.Tools"
            );

        FToolMenuSection& Section =
            Menu->FindOrAddSection("FBXReimportAssistant");

        Section.AddMenuEntry(
            "OpenFBXReimportAssistant",
            FText::FromString("FBX Reimport Assistant"),
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

    void OpenWindow()
    {
        // ---------------------------------
        // Content Browserから選択アセットを取得
        // ---------------------------------

        // 「Content Browserの機能を取得してください。取得できなければエラーにしてください」
        FContentBrowserModule& ContentBrowserModule =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(
                TEXT("ContentBrowser")
            );
        
        // 選択アセットを格納
        TArray<FAssetData> SelectedAssets;
        
        // 選択アセットを取得
        ContentBrowserModule.Get().GetSelectedAssets(
            SelectedAssets
        );


        // ---------------------------------
        // Static Meshを判定・取得
        // ---------------------------------

        int32 StaticMeshCount = 0;
        int32 OtherAssetCount = 0;

        FString MeshNames;

        for (const FAssetData& AssetData : SelectedAssets)
        {
            // アセット本体を取得
            UObject* Asset = AssetData.GetAsset();

            // Static Meshかどうか判定
            UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);

            if (StaticMesh == nullptr)
            {
                ++OtherAssetCount;
                continue;
            }

            // Static Meshとして取得成功
            ++StaticMeshCount;

            MeshNames += FString::Printf(
                TEXT("・%s\n"),
                *StaticMesh->GetName()
            );
        }


        // ---------------------------------
        // ウィンドウに表示する文字列を作成
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
                "選択中にStatic Meshはありません。"
            );
        }
        else
        {
            WindowText += TEXT("取得したStatic Mesh:\n");
            WindowText += MeshNames;
        }


        // ---------------------------------
        // Slateウィンドウを生成
        // ---------------------------------

        TSharedRef<SWindow> Window =
            SNew(SWindow)
            .Title(
                FText::FromString(
                    "FBX Reimport Assistant"
                )
            )
            .ClientSize(FVector2D(500.0f, 300.0f));

        Window->SetContent(
            SNew(STextBlock)
            .Text(FText::FromString(WindowText))
            .AutoWrapText(true)
        );

        FSlateApplication::Get().AddWindow(Window);
    }
};

IMPLEMENT_MODULE(
    FFBXReimportAssistantModule,
    FBXReimportAssistant
)
