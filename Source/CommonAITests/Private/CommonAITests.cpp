#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

class FCommonAITestsModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CommonAI"));
        if (Plugin)
            UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config/Tests/Tags"));
    }
};

IMPLEMENT_MODULE(FCommonAITestsModule, CommonAITests)
