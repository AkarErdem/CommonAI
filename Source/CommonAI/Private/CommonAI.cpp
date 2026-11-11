#include "Modules/ModuleManager.h"
#include "Diagnostics/CommonAIDailyLifeDebug.h"
#include "CommonAILog.h"

DEFINE_LOG_CATEGORY(LogCommonAI);

class FCommonAIModule : public IModuleInterface
{
public:
    virtual void StartupModule() override { CommonAIDailyLifeDebug::Startup(); }
    virtual void ShutdownModule() override { CommonAIDailyLifeDebug::Shutdown(); }
};
IMPLEMENT_MODULE(FCommonAIModule, CommonAI)
