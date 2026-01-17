/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h" 
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_hidh.h"
#include "esp_hid_gap.h"
#include "esp_timer.h"
#include "xbox_pad.h"
#include "rg_input.h"

static const char *TAG = "xbox";

// 连接状态标志
static volatile bool s_is_connected = false;
static volatile bool s_is_connecting = false;
static uint32_t g_xbox_state = 0;

#define RSSI_THRESHOLD -90     
#define NVS_NAMESPACE "xbox_cfg"
#define NVS_KEY_ADDR  "last_bda"
#define NVS_KEY_TYPE  "last_type"

// 保存已连接设备的 MAC 地址到 NVS
void save_device_to_nvs(uint8_t *bda, esp_ble_addr_type_t addr_type) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = 6;
        uint8_t old_bda[6];
        err = nvs_get_blob(my_handle, NVS_KEY_ADDR, old_bda, &required_size);
        
        if (err != ESP_OK || memcmp(old_bda, bda, 6) != 0) {
            nvs_set_blob(my_handle, NVS_KEY_ADDR, bda, 6);
            nvs_set_u8(my_handle, NVS_KEY_TYPE, (uint8_t)addr_type);
            nvs_commit(my_handle);
            ESP_LOGI(TAG, "Saved Xbox MAC to NVS");
        }
        nvs_close(my_handle);
    }
}

// 从 NVS 读取上次连接的设备
bool load_device_from_nvs(uint8_t *bda, esp_ble_addr_type_t *addr_type) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        size_t size = 6;
        err = nvs_get_blob(my_handle, NVS_KEY_ADDR, bda, &size);
        if (err == ESP_OK) {
            uint8_t type_u8;
            nvs_get_u8(my_handle, NVS_KEY_TYPE, &type_u8);
            *addr_type = (esp_ble_addr_type_t)type_u8;
            nvs_close(my_handle);
            return true;
        }
        nvs_close(my_handle);
    }
    return false;
}

void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event)
    {
    case ESP_HIDH_OPEN_EVENT:
    {
        s_is_connecting = false; 
        if (param->open.status == ESP_OK)
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, "OPEN SUCCESS: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(bda));
            s_is_connected = true;
            
            // [修改点 1] 移除了这里会导致报错的 save_device_to_nvs 调用
            // 因为这里获取不到 addr_type，改到 hid_task 里去保存
        }
        else
        {
            ESP_LOGE(TAG, "OPEN FAILED: %d", param->open.status);
            s_is_connected = false;
        }
        break;
    }
    case ESP_HIDH_INPUT_EVENT:
    {
        const uint8_t *data = param->input.data;
        uint16_t length = param->input.length;

        if (length < 14) break; 

        uint16_t joyLX = (data[1] << 8) | data[0]; 
        uint16_t joyLY = (data[3] << 8) | data[2]; 
        uint8_t dpadRaw = data[12];
        uint8_t mainBtns = data[13];
        uint8_t centerBtns = data[14];
        uint8_t shareBtn = data[15];
        
        uint32_t temp_state = 0;

        // 1. 摇杆映射 (模拟量转数字量)
        // Xbox 摇杆范围 0-65535, 中心 ~32768
        if (joyLY < 15000) temp_state |= RG_KEY_UP;
        if (joyLY > 50000) temp_state |= RG_KEY_DOWN;
        if (joyLX < 15000) temp_state |= RG_KEY_LEFT;
        if (joyLX > 50000) temp_state |= RG_KEY_RIGHT;

        // 2. D-Pad 映射
        // 1=上, 2=右上, 3=右, ... 8=左上
        if (dpadRaw == 1 || dpadRaw == 2 || dpadRaw == 8) temp_state |= RG_KEY_UP;
        if (dpadRaw >= 2 && dpadRaw <= 4) temp_state |= RG_KEY_RIGHT;
        if (dpadRaw >= 4 && dpadRaw <= 6) temp_state |= RG_KEY_DOWN;
        if (dpadRaw >= 6 && dpadRaw <= 8) temp_state |= RG_KEY_LEFT;

        // 3. 按键映射 (根据 config.h/rg_input.h 的习惯)
        // A -> A, B -> B, X -> Y, Y -> X (Retro-Go 常见布局习惯，可根据需要互换)
        if (mainBtns & 0x01) temp_state |= RG_KEY_A;      // Xbox A
        if (mainBtns & 0x02) temp_state |= RG_KEY_B;      // Xbox B
        if (mainBtns & 0x08) temp_state |= RG_KEY_Y;      // Xbox X -> RG Y
        if (mainBtns & 0x10) temp_state |= RG_KEY_X;      // Xbox Y -> RG X
        
        // 肩键
        if (mainBtns & 0x40) temp_state |= RG_KEY_L;      // LB
        if (mainBtns & 0x80) temp_state |= RG_KEY_R;      // RB

        // 功能键
        if (centerBtns & 0x04) temp_state |= RG_KEY_SELECT; // View
        if (centerBtns & 0x08) temp_state |= RG_KEY_START;  // Menu
        if (centerBtns & 0x10) temp_state |= RG_KEY_MENU;   // Xbox Home -> RG Menu

        if (shareBtn & 0x01) temp_state |= RG_KEY_OPTION;   //Xbox Share -> RG Option

        // 功能键
        g_xbox_state = temp_state;
        break;
    }
    case ESP_HIDH_CLOSE_EVENT:
    {
        ESP_LOGI(TAG, "CLOSE");
        s_is_connected = false;
        s_is_connecting = false;
        g_xbox_state = 0;
        break;
    }
    default:
        break;
    }
}

void hid_task(void *pvParameters)
{
    // ---------------------------------------------------------
    // 阶段1：极速重连
    // ---------------------------------------------------------
    uint8_t last_bda[6];
    esp_ble_addr_type_t last_type;
    
    if (load_device_from_nvs(last_bda, &last_type)) {
        ESP_LOGI(TAG, "Found saved Xbox: " ESP_BD_ADDR_STR ", trying Direct Connect...", ESP_BD_ADDR_HEX(last_bda));
        
        s_is_connecting = true;
        esp_hidh_dev_open(last_bda, ESP_HID_TRANSPORT_BLE, last_type);
        
        int timeout = 40; 
        while (s_is_connecting && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            timeout--;
        }

        if (s_is_connected) {
            ESP_LOGI(TAG, "Fast Reconnect Success!");
        } else {
            ESP_LOGW(TAG, "Fast Reconnect Failed/Timeout, switching to Scan mode.");
            s_is_connecting = false;
        }
    }

    // ---------------------------------------------------------
    // 阶段2：常规扫描
    // ---------------------------------------------------------
    while (1) {
        if (s_is_connected) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        if (s_is_connecting) {
             vTaskDelay(pdMS_TO_TICKS(500));
             continue;
        }

        size_t results_len = 0;
        esp_hid_scan_result_t *results = NULL;
        ESP_LOGI(TAG, "Scanning...");
        
        esp_hid_scan(2, &results_len, &results);
        
        if (results_len) {
            esp_hid_scan_result_t *r = results;
            esp_hid_scan_result_t *target_dev = NULL;
            
            while (r) {
                if (r->transport == ESP_HID_TRANSPORT_BLE) {
                    if (r->name && strstr(r->name, "Xbox") && r->rssi > RSSI_THRESHOLD) {
                        target_dev = r;
                        break; 
                    }
                }
                r = r->next;
            }

            if (target_dev) {
                ESP_LOGI(TAG, "Target found: %s. Connecting...", target_dev->name);
                s_is_connecting = true;
                
                // [修改点 2] 在发起连接前，直接保存到 NVS
                // 此时我们拥有 target_dev->ble.addr_type，不会报错
                save_device_to_nvs(target_dev->bda, target_dev->ble.addr_type);

                esp_hidh_dev_open(target_dev->bda, target_dev->transport, target_dev->ble.addr_type);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            esp_hid_scan_results_free(results);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelete(NULL);
}

void xbox_pad_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_hid_gap_init(HIDH_BLE_MODE));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));
    
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(esp_hidh_init(&config));

    //xTaskCreate(&hid_task, "xbox_scan", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(&hid_task, "xbox_scan", 6 * 1024, NULL, 7, NULL, 0);
}

uint32_t xbox_pad_get_state(void)
{
    return g_xbox_state;
}