#include "nvs_flash.h"
#include "SystemAPI/GPU/GpuDriver.h"
#include "SystemAPI/Web/Server/WifiManager.hpp"
#include "SystemAPI/Web/Server/DnsServer.hpp"
#include "SystemAPI/Misc/SyncState.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <cstring>
#include <cmath>
#include <vector>

static const char* TAG = "WIFI_SPRITE_TEST";

using namespace SystemAPI;
using namespace SystemAPI::Web;

// ...existing code...

extern "C" void app_main() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ...rest of your initialization...
}
