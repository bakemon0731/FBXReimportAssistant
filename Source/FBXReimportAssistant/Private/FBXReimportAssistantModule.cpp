#include "FBXReimportAssistantModule.h"

#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#include "Widgets/SFBXReimportHomeWindow.h"
#include "Widgets/SFBXReimportWindow.h"
#include "Widgets/SFBXBatchReimportWindow.h"
#include "Styles/FBXReimportAssistantStyle.h"

void FFBXReimportAssistantModule::StartupModule()
{
    FFBXReimportAssistantStyle::Initialize();

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this,
            &FFBXReimportAssistantModule::RegisterMenus
        )
    );
}

void FFBXReimportAssistantModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    MainWindow.Reset();
    FFBXReimportAssistantStyle::Shutdown();
}

void FFBXReimportAssistantModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // Toolsメニューには個別ツールではなく、共通ホーム画面だけを登録します。
    UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(
        TEXT("LevelEditor.MainMenu.Tools")
    );

    FToolMenuSection& ToolsSection = ToolsMenu->FindOrAddSection(
        TEXT("FBXReimportAssistant")
    );

    ToolsSection.AddMenuEntry(
        TEXT("OpenFBXReimportAssistantHome"),
        FText::FromString(TEXT("FBX Reimport Assistant")),
        FText::FromString(TEXT("FBX Reimport Assistantのホーム画面を開きます。")),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh")),
        FUIAction(FExecuteAction::CreateRaw(
            this,
            &FFBXReimportAssistantModule::OpenHomeWindow
        ))
    );

    // PIEボタン付近のPlayツールバーに、ホーム画面を開くショートカットを追加します。
    UToolMenu* PlayToolbar = UToolMenus::Get()->ExtendMenu(
        TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar")
    );

    FToolMenuSection& ToolbarSection = PlayToolbar->FindOrAddSection(
        TEXT("FBXReimportAssistant")
    );

    ToolbarSection.AddEntry(
        FToolMenuEntry::InitToolBarButton(
            TEXT("OpenFBXReimportAssistantToolbar"),
            FUIAction(FExecuteAction::CreateRaw(
                this,
                &FFBXReimportAssistantModule::OpenHomeWindow
            )),
            FText::FromString(TEXT("FBX Tool")),
            FText::FromString(TEXT("FBX Reimport Assistantのホーム画面を開きます。")),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh"))
        )
    );
}

void FFBXReimportAssistantModule::OpenHomeWindow()
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    const bool bCreateWindow = !MainWindow.IsValid();
    if (bCreateWindow)
    {
        MainWindow = SNew(SWindow)
            .Title(FText::FromString(TEXT("FBX Reimport Assistant")))
            .ClientSize(FVector2D(900.0f, 600.0f))
            .SupportsMinimize(true)
            .SupportsMaximize(true);
        // ウィンドウクローズ時のイベントを登録
        MainWindow->GetOnWindowClosedEvent().AddRaw(
            this,
            &FFBXReimportAssistantModule::HandleMainWindowClosed
        );
            
    }

    MainWindow->SetTitle(FText::FromString(TEXT("FBX Reimport Assistant")));
    MainWindow->SetContent(
        SNew(SFBXReimportHomeWindow)
        .OnOpenSingleTool(FSimpleDelegate::CreateRaw(
            this,
            &FFBXReimportAssistantModule::ShowSingleReimportTool
        ))
        .OnOpenBatchTool(FSimpleDelegate::CreateRaw(
            this,
            &FFBXReimportAssistantModule::ShowBatchReimportTool
        ))
    );

    if (bCreateWindow)
    {
        FSlateApplication::Get().AddWindow(MainWindow.ToSharedRef());
    }

    MainWindow->BringToFront(true);
}

void FFBXReimportAssistantModule::ShowSingleReimportTool()
{
    if (!MainWindow.IsValid())
    {
        OpenHomeWindow();
        return;
    }

    MainWindow->SetTitle(FText::FromString(TEXT("FBX Reimport Assistant - 単体")));
    MainWindow->Resize(FVector2D(700.0f, 700.0f));
    MainWindow->SetContent(
        SNew(SFBXReimportWindow)
        .OnBackToHome(FSimpleDelegate::CreateRaw(
            this,
            &FFBXReimportAssistantModule::OpenHomeWindow
        ))
    );
}

void FFBXReimportAssistantModule::ShowBatchReimportTool()
{
    if (!MainWindow.IsValid())
    {
        OpenHomeWindow();
        return;
    }

    MainWindow->SetTitle(FText::FromString(TEXT("FBX Reimport Assistant - 一括")));
    MainWindow->Resize(FVector2D(850.0f, 760.0f));
    MainWindow->SetContent(
        SNew(SFBXBatchReimportWindow)
        .OnBackToHome(FSimpleDelegate::CreateRaw(
            this,
            &FFBXReimportAssistantModule::OpenHomeWindow
        ))
    );
}

void FFBXReimportAssistantModule::HandleMainWindowClosed(const TSharedRef<SWindow>& Window)
{
    if (MainWindow.IsValid() && MainWindow.Get() == &Window.Get())
    {
        MainWindow.Reset();
    }
}

IMPLEMENT_MODULE(FFBXReimportAssistantModule, FBXReimportAssistant)
