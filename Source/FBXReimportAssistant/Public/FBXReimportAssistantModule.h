#pragma once

#include "Modules/ModuleManager.h"

class FFBXReimportAssistantModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();

    void OpenSingleReimportWindow();
    void OpenBatchReimportWindow();
};
