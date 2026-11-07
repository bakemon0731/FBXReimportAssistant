#pragma once

#include "Modules/ModuleManager.h"

class SWindow;

class FFBXReimportAssistantModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();

    void OpenHomeWindow();
    void ShowSingleReimportTool();
    void ShowBatchReimportTool();
    void HandleMainWindowClosed(const TSharedRef<SWindow>& Window);

    TSharedPtr<SWindow> MainWindow;
};
