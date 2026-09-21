#include "Modules/ModuleManager.h"
#include "Testing/RatwScenario.h"

class FRatwModule : public FDefaultGameModuleImpl
{
  public:
    virtual void StartupModule() override
    {
        RegisterRatwScenario();
    }
    virtual void ShutdownModule() override
    {
        UnregisterRatwScenario();
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FRatwModule, RATWMUD, "RATWMUD");
