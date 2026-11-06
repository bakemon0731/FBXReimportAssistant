
#include "Modules/ModuleManager.h"

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
            .Text(
                FText::FromString(
                    "FBX Reimport Assistant"
                )
            )
        );

        FSlateApplication::Get().AddWindow(Window);
    }
};

IMPLEMENT_MODULE(
    FFBXReimportAssistantModule,
    FBXReimportAssistant
)
