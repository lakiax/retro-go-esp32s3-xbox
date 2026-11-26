#include "shared.h"


void app_main(void)
{
    rg_app_t *app = rg_system_init(AUDIO_SAMPLE_RATE, NULL, NULL);

    RG_LOGI("configNs=%s", app->configNs);

    // NES-only build: nofrendo is the only emulator linked into this app.
    // The launcher passes "nes" as configNs when starting a ROM; any other
    // value simply returns to the launcher.
    if (strcmp(app->configNs, "nes") == 0)
        nes_main();
    else
        launcher_main();

    RG_PANIC("Never reached");
}
