#include "mico.h"
#include "mico_app_define.h"
#include "audio_test.h"
#include "hal_alilo_rabbit.h"
#include "audio_service.h"
#define app_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

static mico_semaphore_t wait_sem = NULL;

/* MICO system callback: Restore default configuration provided by application */

static void micoNotify_WifiStatusHandler(WiFiEvent status, void *const inContext)
{
    switch (status)
    {
    case NOTIFY_STATION_UP:
        mico_rtos_set_semaphore(&wait_sem);
        break;
    case NOTIFY_STATION_DOWN:
    case NOTIFY_AP_UP:
    case NOTIFY_AP_DOWN:
        break;
    }
}

void ssl_log(const int logLevel, const char *const logMessage)
{
    app_log("%s\r\n", logMessage);
}

int application_start(void)
{
    app_log_trace();
    OSStatus err = kNoErr;
    mico_Context_t *mico_context;
    app_context_t *app_context;
    mscp_result_t result;
    ADUIO_SYSTEM_FW_INFO_S firmware_info_p;

    /* Create application context */
    app_context = (app_context_t *)calloc(1, sizeof(app_context_t));
    require_action(app_context, exit, err = kNoMemoryErr);

    mico_rtos_init_semaphore(&wait_sem, 1);

    /*Register user function for MiCO nitification: WiFi status changed */
    err = mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED,
                                      (void *)micoNotify_WifiStatusHandler,
                                      NULL);
    require_noerr(err, exit);

    /* Create mico system context and read application's config data from flash */
    mico_context = mico_system_context_init(sizeof(application_config_t));
    app_context->appConfig = mico_system_context_get_user_data(mico_context);

    /* mico system initialize */
    err = mico_system_init(mico_context);
    require_noerr(err, exit);

    err = hal_alilo_rabbit_init();
    require_noerr(err, exit);

    /* Wait for wlan connection*/
    mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);
    app_log("wifi connected successful");

    err = start_test_thread();
    require_noerr(err, exit);

    audio_service_system_get_fw_info(&result, &firmware_info_p);
    app_log("\r\nmx1200_ver = V%d.%d.%d\r\n"
            "sound_remind_ver = V%d.%d.%d\r\n"
            "hardware_ver = V%d.%d\r\n"
            "mx1200_fw_name_len = %d\r\n"
            "mx1200_fw_name[64] = %s\r\n"
            "sound_remind_fw_name_len = %d\r\n"
            "sound_remind_fw_name[64] = %s",
            firmware_info_p.mx1200_ver_major,
            firmware_info_p.mx1200_ver_minor,
            firmware_info_p.mx1200_ver_revision,
            firmware_info_p.sound_remind_ver_major,
            firmware_info_p.sound_remind_ver_minor,
            firmware_info_p.sound_remind_ver_revision,
            firmware_info_p.hardware_ver_major,
            firmware_info_p.hardware_ver_minor,
            firmware_info_p.mx1200_fw_name_len,
            firmware_info_p.mx1200_fw_name,
            firmware_info_p.sound_remind_fw_name_len,
            firmware_info_p.sound_remind_fw_name);

exit:
    mico_system_notify_remove(mico_notify_WIFI_STATUS_CHANGED,
                              (void *)micoNotify_WifiStatusHandler);
    mico_rtos_deinit_semaphore(&wait_sem);
    mico_rtos_delete_thread(NULL);

    return err;
}
