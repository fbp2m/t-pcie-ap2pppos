/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * softAP to PPPoS Example (network_dce)
*/

#include <string.h>
#include "esp_netif.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "network_dce.h"


static esp_modem_dce_t *dce = NULL;
static char * TAG = "network_dce";
static char modemName[32];
int baud_rate = 1842000; //3686400 1842000

static void uart_terminal_error_handler(esp_modem_terminal_error_t err) {
    ESP_LOGE(TAG, "Error in terminal handler: %d", err);
	// Error in terminal handler: 2 <- reçu après AT+CFUN=1,1
	if(err == 2){
		xEventGroupSetBits(*get_event_group(), DISCONNECT_BIT);
	}
}

esp_err_t modem_init_network(esp_netif_t *netif)
{
    // setup the DCE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.baud_rate = 115200;
    dte_config.uart_config.tx_io_num = 27;
    dte_config.uart_config.rx_io_num = 26;
    dte_config.uart_config.rx_buffer_size = 4*1024;
    dte_config.uart_config.tx_buffer_size = 4*512;
    dte_config.uart_config.event_queue_size = 30;
    dte_config.task_stack_size = 4096;
    dte_config.task_priority = configMAX_PRIORITIES - 1;
    dte_config.dte_buffer_size = 1024 * 2;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
    dce = esp_modem_new(&dte_config, &dce_config, netif);
    while(esp_modem_get_module_name(dce,modemName) != ESP_OK){
        ESP_LOGW(TAG, "esp_modem_get_module_name failed");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_err_t err = esp_modem_set_baud(dce, baud_rate);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set baud rate: %d", err);
        return ESP_FAIL;
    }else{
        ESP_LOGI(TAG, "esp_modem_set_baud OK");
        esp_modem_destroy(dce);
        dte_config.uart_config.baud_rate = baud_rate; 
        dce = esp_modem_new(&dte_config, &dce_config, netif);
        assert(dce);
        vTaskDelay(2000 / portTICK_PERIOD_MS); // Modem does not respond if not wait after change baudrate
    }
    while(esp_modem_get_module_name(dce,modemName) != ESP_OK){
        ESP_LOGW(TAG, "esp_modem_get_module_name after changing baudrate failed");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    esp_modem_set_error_cb(dce, uart_terminal_error_handler);
    if (!dce) {
        return ESP_FAIL;
    }

#ifdef CONFIG_EXAMPLE_NEED_SIM_PIN
    // configure the PIN
    bool pin_ok = false;
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok == false) {
        if (esp_modem_set_pin(dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            abort();
        }
    }
#endif // CONFIG_EXAMPLE_NEED_SIM_PIN
    return ESP_OK;
}

void modem_deinit_network(void)
{
    if (dce) {
        esp_modem_destroy(dce);
        dce = NULL;
    }
}

bool modem_start_network()
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA) == ESP_OK;
}

bool modem_stop_network()
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
}

bool modem_check_sync()
{
    return esp_modem_sync(dce) == ESP_OK;
}

void modem_reset()
{
    esp_modem_reset(dce);
}

bool modem_check_signal()
{
    int rssi, ber;
    if (esp_modem_get_signal_quality(dce, &rssi, &ber) == ESP_OK) {
        return rssi != 99 && rssi > 5;
    }
    return false;
}