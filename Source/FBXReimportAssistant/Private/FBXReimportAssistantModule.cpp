#include "FBXReimportAssistantModule.h"

#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#include "Widgets/SFBXReimportWindow.h"
#include "Widgets/SFBXBatchReimportWindow.h"

void FFBXReimportAssistantModule::StartupModule()
{
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
}

void FFBXReimportAssistantModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(
        TEXT("LevelEditor.MainMenu.Tools")
    );

    FToolMenuSection& Section = Menu->FindOrAddSection(
        TEXT("FBXReimportAssistant")
    );

    Section.AddMenuEntry(
        TEXT("OpenFBXReimportAssistant"),
        FText::FromString(TEXT("FBX Reimport Assistant")),
        FText::FromString(TEXT("Open single FBX reimport tool")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(
            this,
            &FFBXReimportAssistantModule::OpenSingleReimportWindow
        ))
    );

    Section.AddMenuEntry(
        TEXT("OpenFBXBatchReimportAssistant"),
        FText::FromString(TEXT("FBX Batch Reimport Assistant")),
        FText::FromString(TEXT("Reimport multiple FBX Static Mesh assets")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(
            this,
            &FFBXReimportAssistantModule::OpenBatchReimportWindow
        ))
    );
}

void FFBXReimportAssistantModule::OpenSingleReimportWindow()
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("FBX Reimport Assistant")))
        .ClientSize(FVector2D(700.0f, 700.0f));

    Window->SetContent(SNew(SFBXReimportWindow));
    FSlateApplication::Get().AddWindow(Window);
}

void FFBXReimportAssistantModule::OpenBatchReimportWindow()
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("FBX Batch Reimport Assistant")))
        .ClientSize(FVector2D(800.0f, 650.0f));

    Window->SetContent(SNew(SFBXBatchReimportWindow));
    FSlateApplication::Get().AddWindow(Window);
}

IMPLEMENT_MODULE(FFBXReimportAssistantModule, FBXReimportAssistant)
