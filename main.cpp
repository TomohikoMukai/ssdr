#include "SampleApp.h"
#include <crtdbg.h>
#include "HorseObject.h"

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    SampleApp::Config config;
    config.title = L"DemoSSDR";

    SampleApp app(config);

    HorseObject::SharedPtr horse = HorseObject::CreateInstance();
    app.AddObject(horse);

    return app.Run();
}

