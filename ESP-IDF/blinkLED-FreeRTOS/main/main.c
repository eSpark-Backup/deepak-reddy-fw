#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "string.h"
#include "stdlib.h"
#include "cJSON.h"
#include "esp_spiffs.h"


#define led_pin GPIO_NUM_4
#define Relay1 5
#define Relay2 2

static const int rate_1 = 500;  // ms
static const int rate_2 = 323;  // ms

#define HIGH 1
#define LOW 0

#define CONFIG_FREERTOS_UNICORE 1

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "wifi station";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define EXAMPLE_ESP_WIFI_SSID      "KALBEABBAS 1096"
#define EXAMPLE_ESP_WIFI_PASS      "UsmaneSpark"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

static int s_retry_num = 0;
extern const char ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const char ca_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

SemaphoreHandle_t xSemaphore = NULL;

double epoch = 0;
int dur = 0;


static void obtain_time(void);
static void initialize_sntp(void);

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Our task: blink an LED at one rate
void toggleLED_1(void *parameter) {
  while(1) {
	gpio_set_level(led_pin, HIGH);
    vTaskDelay(rate_1 / portTICK_PERIOD_MS);
    gpio_set_level(led_pin, LOW);
    vTaskDelay(rate_1 / portTICK_PERIOD_MS);

    /*esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(IP_EVENT_STA_GOT_IP,&ip_info);
    printf("My IP: " IPSTR "\n", IP2STR(&ip_info.ip));
    printf("My GW: " IPSTR "\n", IP2STR(&ip_info.gw));
    printf("My NETMASK: " IPSTR "\n", IP2STR(&ip_info.netmask));*/
  }
}

// Our task: blink an LED at another rate
void toggleLED_2(void *parameter) {
  while(1) {
    gpio_set_level(led_pin, HIGH);
    vTaskDelay(rate_2 / portTICK_PERIOD_MS);
    gpio_set_level(led_pin, LOW);
    vTaskDelay(rate_2 / portTICK_PERIOD_MS);
  }
}

void wifi_init_sta(void)
{

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    printf("wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
    	printf("connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
    	printf("Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
    	printf("UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len = 0;       // Stores number of bytes read

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            //printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {

                    memcpy(evt->user_data + output_len * sizeof(char), evt->data, evt->data_len);
                    /*printf("Data\n");
                    printf("%s\n", (char*)evt->data);*/
                }
//                } else {
//                    if (output_buffer == NULL) {
//                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
//                        output_len = 0;
//                        if (output_buffer == NULL) {
//                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
//                            return ESP_FAIL;
//                        }
//                    }
//                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
//                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;

            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
    }
    return ESP_OK;
}

int getToday(char* today)
{
	int w_day = 0;
	if(strncmp(today, "Monday", 6) == 0)
	{
		w_day = 1;
	}else if(strncmp(today, "Tuesday", 7) == 0)
	{
		w_day = 2;
	}else if(strncmp(today, "Wednesday", 9) == 0)
	{
		w_day = 3;
	}else if(strncmp(today, "Thursday", 8) == 0)
	{
		w_day = 4;
	}else if(strncmp(today, "Friday", 6) == 0)
	{
		w_day = 5;
	}else if(strncmp(today, "Saturday", 8) == 0)
	{
		w_day = 6;
	}else if(strncmp(today, "Sunday", 6) == 0)
	{
		w_day = 0;
	}else{
		w_day = -1;
	}

	return w_day;
}

void getSchedule(double* epoch, int * dur, int w_day)
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "Reading file");
    FILE* f = fopen("/spiffs/schedule.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }

    char line[2000];
	fread(line, sizeof(line), 1, f);


    fclose(f);


    ESP_LOGI(TAG, "Read from file: '%s'", line);

    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");

    cJSON* parse_json = cJSON_Parse(line);

    if (parse_json == NULL)
    {
    	printf("Error parse_json\n");
    }

    cJSON* schedule = cJSON_GetObjectItem(parse_json, "Schedule");

    int array_size = cJSON_GetArraySize(schedule);
    printf("array size %d\n", array_size);

    for(int i = 0; i < array_size; i++)
    {
    	cJSON* array = cJSON_GetArrayItem(schedule, i);
        if (array == NULL)
        {
        	printf("Error array\n");
        }

    	cJSON* day = cJSON_GetObjectItem(array, "Day");
        if (day == NULL)
        {
        	printf("Error day\n");
        }

        char* today = day->valuestring;
    	printf("Day %s\n", today);
    	if(getToday(today) == w_day)
    	{
    		*epoch = (cJSON_GetObjectItem(array, "Start Time(epoch)")->valuedouble);
    		*dur = cJSON_GetObjectItem(array, "Duration(minutes)")->valueint;

    		break;
    	}
    }
}

void storeToSPIFFS(char* string)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen("/spiffs/schedule.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f, string);
    fclose(f);
    ESP_LOGI(TAG, "File written");


//    // Open schedule.txt file for reading
//    ESP_LOGI(TAG, "Reading file");
//    f = fopen("/spiffs/schedule.txt", "r");
//    if (f == NULL) {
//        ESP_LOGE(TAG, "Failed to open file for reading");
//        return;
//    }
//
//    char line[2000];
//	fread(line, sizeof(line), 1, f);
//
//
//    fclose(f);
//
//    ESP_LOGI(TAG, "Read from file: '%s'", line);
//
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
//
//    cJSON* parse_json = cJSON_Parse(line);
//
//    if (parse_json == NULL)
//    {
//    	printf("Error parse_json\n");
//    }
//
//    cJSON* schedule = cJSON_GetObjectItem(parse_json, "Schedule");
//
//    int array_size = cJSON_GetArraySize(schedule);
//    printf("array size %d\n", array_size);
//
//    for(int i = 0; i < array_size; i++)
//    {
//    	cJSON* array = cJSON_GetArrayItem(schedule, i);
//        if (array == NULL)
//        {
//        	printf("Error array\n");
//        }
//
//    	cJSON* day = cJSON_GetObjectItem(array, "Day");
//        if (day == NULL)
//        {
//        	printf("Error day\n");
//        }
//
//    	printf("Day %s\n", day->valuestring);
//    }

//    cJSON* schedule = cJSON_GetObjectItem(parse_json, "Schedule");
//    if (schedule == NULL)
//    {
//    	printf("Error in schedule JSON\n");
//    }
//
//    cJSON* schedule1 = cJSON_GetObjectItem(schedule, "Schdule01");
//    if (schedule1 == NULL)
//    {
//    	printf("Error in schedule1 JSON\n");
//    }
//
//    cJSON* day = cJSON_GetObjectItem(schedule1, "Day");
//    if (day == NULL)
//    {
//    	printf("Error in Day JSON\n");
//    }
//
//    printf("Day: %s\n", day->valuestring);


//    cJSON* childJSON = NULL;
//    cJSON* schedule1 = NULL;
//    cJSON* schedule1_child = NULL;
//    cJSON* schedule1_day = NULL;
//
//    childJSON = schedule->child;
//    if (childJSON == NULL)
//    {
//        const char *error_ptr = cJSON_GetErrorPtr();
//        if (error_ptr != NULL)
//        {
//            printf("Error: %s\n", error_ptr);
//        }
//    }
//
//    schedule1 = cJSON_GetObjectItem(childJSON,"Schdule01");
//    if (schedule1 == NULL)
//    {
//        const char *error_ptr = cJSON_GetErrorPtr();
//        if (error_ptr != NULL)
//        {
//            printf("Error: %s\n", error_ptr);
//        }
//    }
//
//    schedule1_child = schedule1->child;
//    if (schedule1_child == NULL)
//    {
//        const char *error_ptr = cJSON_GetErrorPtr();
//        if (error_ptr != NULL)
//        {
//            printf("Error: %s\n", error_ptr);
//        }
//    }
//
//    schedule1_day = cJSON_GetObjectItem(schedule1_child, "Day");
//    if (schedule1_day == NULL)
//    {
//        const char *error_ptr = cJSON_GetErrorPtr();
//        if (error_ptr != NULL)
//        {
//            printf("Error: %s\n", error_ptr);
//        }
//    }
//
//    printf("Parsed string %s\n", schedule1_day->valuestring);





}

void getJSON()
{
	while(1)
	{
		if(xSemaphore != NULL)
		{
			if(xSemaphoreTake(xSemaphore, (TickType_t) (10000 / portTICK_PERIOD_MS)) == pdTRUE)
			{
				char local_response_buffer[2000] = {0};

				esp_http_client_config_t config = {
					.url = "https://esp-idf-bucket.s3.us-west-1.amazonaws.com/schedule.json",
					.event_handler = _http_event_handle,
					.cert_pem = ca_cert_pem_start,
					.method = HTTP_METHOD_GET,
					.user_data = local_response_buffer,
					.buffer_size = 1024,
				};

				esp_http_client_handle_t client = esp_http_client_init(&config);
				//esp_err_t err = esp_http_client_open(client, 0);

				esp_err_t err = esp_http_client_perform(client);

				if (err == ESP_OK) {

					int content_len = esp_http_client_get_content_length(client);

				   ESP_LOGI(TAG, "Status = %d, content_length = %d",
						   esp_http_client_get_status_code(client),
						   content_len);


				   storeToSPIFFS(local_response_buffer);


				   printf("Response\n");
				   //printf("%s\n", local_response_buffer);

				   for(int i = 0; i < 2000; i++)
				   {
					   local_response_buffer[i] = 0;
				   }

				}
				esp_http_client_cleanup(client);

				xSemaphoreGive(xSemaphore);

				vTaskDelay(10000 / portTICK_PERIOD_MS);
			}
		}
	}

}

void getTime()
{
	while(1)
	{
		if(xSemaphore != NULL)
		{
			if(xSemaphoreTake(xSemaphore, (TickType_t)(10000 / portTICK_PERIOD_MS)) == pdTRUE)
			{
				//char strftime_buf[64];

				printf("Update Time\n");
				time_t now;
				struct tm timeinfo;
				time(&now);
				localtime_r(&now, &timeinfo);

				obtain_time();
				// update 'now' variable with current time
				time(&now);

				ESP_LOGI(TAG, "Year = %d, Month = %d, Day = %d", timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_wday);
				ESP_LOGI(TAG, "EPOCH = %ld", now);

				int w_day = timeinfo.tm_wday;

				// Set timezone and print local time
		//	    setenv("TZ", "UTC-5", 1);
		//	    tzset();
		//	    localtime_r(&now, &timeinfo);
		//	    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		//	    ESP_LOGI(TAG, "The current date/time in Pakistan is: %s", strftime_buf);

				getSchedule(&epoch, &dur, w_day);

				xSemaphoreGive(xSemaphore);

				vTaskDelay(10000 / portTICK_PERIOD_MS);
			}
		}
	}

}

void executeSchedule()
{
	while(1)
	{
		time_t now;
		time(&now);

		printf("Epoch: %lf, Dur: %d", epoch, dur);

		if((now >= epoch) && (now < (epoch + dur * 60)))
		{
			printf("\nActuate relays\n");

	        gpio_set_level(Relay1, 1);
	        gpio_set_level(Relay2, 1);
		}else{
			printf("\nOut of schedule\n");

	        gpio_set_level(Relay1, 0);
	        gpio_set_level(Relay2, 0);
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}


static void obtain_time(void)
{
    /*ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK( esp_event_loop_create_default() );*/

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 3;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    sntp_init();
}


void app_main(void)
{
    nvs_flash_init();

    wifi_init_sta();

    esp_netif_init();

    initialize_sntp();

    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    xSemaphore = xSemaphoreCreateBinary();

    xSemaphoreGive(xSemaphore);


    gpio_reset_pin(Relay1);
    gpio_reset_pin(Relay2);

    gpio_set_direction(Relay1, GPIO_MODE_OUTPUT);
    gpio_set_direction(Relay2, GPIO_MODE_OUTPUT);

    // Task to run forever
    xTaskCreatePinnedToCore(  // Use xTaskCreate() in vanilla FreeRTOS
                toggleLED_1,  // Function to be called
                "Toggle 1",   // Name of task
                1024,         // Stack size (bytes in ESP32, words in FreeRTOS)
                NULL,         // Parameter to pass to function
                0,            // Task priority (0 to configMAX_PRIORITIES - 1)
                NULL,         // Task handle
                app_cpu);     // Run on one core for demo purposes (ESP32 only)

    xTaskCreatePinnedToCore(  // Use xTaskCreate() in vanilla FreeRTOS
    		     getJSON,  // Function to be called
                "Get Schedule",   // Name of task
                1024 * 10,         // Stack size (bytes in ESP32, words in FreeRTOS)
                NULL,         // Parameter to pass to function
                0,            // Task priority (0 to configMAX_PRIORITIES - 1)
                NULL,         // Task handle
                app_cpu);     // Run on one core for demo purposes (ESP32 only)

    xTaskCreatePinnedToCore(  // Use xTaskCreate() in vanilla FreeRTOS
    		     getTime,  // Function to be called
                "Get Time",   // Name of task
                1024 * 4,         // Stack size (bytes in ESP32, words in FreeRTOS)
                NULL,         // Parameter to pass to function
                0,            // Task priority (0 to configMAX_PRIORITIES - 1)
                NULL,         // Task handle
                app_cpu);     // Run on one core for demo purposes (ESP32 only)

    xTaskCreatePinnedToCore(  // Use xTaskCreate() in vanilla FreeRTOS
    		     executeSchedule,  // Function to be called
                "ExecuteSchedule",   // Name of task
                1024 * 2,         // Stack size (bytes in ESP32, words in FreeRTOS)
                NULL,         // Parameter to pass to function
                0,            // Task priority (0 to configMAX_PRIORITIES - 1)
                NULL,         // Task handle
                app_cpu);     // Run on one core for demo purposes (ESP32 only)

    // Task to run forever
//    xTaskCreatePinnedToCore(  // Use xTaskCreate() in vanilla FreeRTOS
//                toggleLED_2,  // Function to be called
//                "Toggle 2",   // Name of task
//                1024,         // Stack size (bytes in ESP32, words in FreeRTOS)
//                NULL,         // Parameter to pass to function
//                0,            // Task priority (0 to configMAX_PRIORITIES - 1)
//                NULL,         // Task handle
//                app_cpu);     // Run on one core for demo purposes (ESP32 only)

    // If this was vanilla FreeRTOS, you'd want to call vTaskStartScheduler() in
    // main after setting up your tasks.


}

