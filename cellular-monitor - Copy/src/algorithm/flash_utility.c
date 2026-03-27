/**
 ******************************************************************************
 * @file    flash_utility.c
 * @author  Charles Fayal
 * @brief   Utility for accessing flash memory
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2024 NOWi Sensors
 * All rights reserved.</center></h2>
 *
 *
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include "algorithm.h"
#include "flash_utility.h"
#include "app.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(flash_utility, CONFIG_APP_LOG_LEVEL);

static struct nvs_fs fs;
struct flash_pages_info info;

#define NVS_PARTITION storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)

/* Flash data ids*/
#define HLF_CYCLE_ID 1          /* half cycles flash keyid */
#define LFOPTD_FS_ID 2          /* LowestFlowOverPeriodTracker flash keyid */
#define CONFIGURATION_FS_ID 3   /* configurations flash keyid */
#define DEVICE_ID_FS_ID 4       /* device id flash keyid */
#define PROVISIONING_STATE_ID 5 /* provisioning state flash keyid */
#define DEVICE_STATE_ID 6       /* device state flash keyid */
#define DEVICE_TYPE_ID 7        /* device type flash keyid */
#define LOGGING_STATE_ID 8      /* logging state flash keyid */

#define MAX_VARIABLE_LENGTH 30 // 32 - 1 for header - 1 for length

static bool flash_initialized = false;
/**
 * @brief Converts a float to a uint32_t
 *
 * @param[in] f        The float to convert
 * @retval uint32_t    The converted value'
 * @note This function multiplies by 1000 to maintain 3 decimal places

 */
uint32_t FloatToUInt32(float input)
{
    return input * 1000.0f;
}

/**
 * @brief Converts a uint32_t to a float
 *
 * @param[in] i        The uint32_t to convert
 * @retval float       The converted value
 * @note This function divides by 1000 to get 3 decimal places
 */
float Uint32ToFloat(uint32_t input)
{
    return input / 1000.0f;
}

/**
 * @brief Initialize the flash space.
 *
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_init(void)
{
    int rc = 0;

    if (flash_initialized == true)
        return FlashStatus_Success;

    LOG_INF("FlashUtility_init");

    fs.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(fs.flash_device))
    {
        LOG_ERR("Flash device %s is not ready", fs.flash_device->name);
        return FlashStatus_Failure;
    }

    fs.offset = NVS_PARTITION_OFFSET;
    rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (rc)
    {
        LOG_ERR("Unable to get page info");
        return FlashStatus_Failure;
    }
    fs.sector_size = info.size;
    fs.sector_count = 3U;
    // With nvs_write wear leveling is implemented, so sector size and count figure out lifespan
    LOG_INF("sector_size %d, sector_count %d", fs.sector_size, fs.sector_count);

    rc = nvs_mount(&fs);
    if (rc)
    {
        LOG_ERR("Flash Init failed");
        return FlashStatus_Failure;
    }

    // LOG_INF("nvs_calc_free_space() %d ", nvs_calc_free_space(&fs));

    flash_initialized = true;

    return FlashStatus_Success;
}

/**
 * @brief Saves the device ID to flash.
 *
 * @param[in] deviceID           The device ID to save
 * @param[in] deviceIDLength     The length of the device ID
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveDeviceID(char *deviceID, uint8_t deviceIDLength)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_SaveDeviceID not initialized");
        return FlashStatus_Failure;
    }
    int rc = nvs_write(&fs, DEVICE_ID_FS_ID, deviceID, deviceIDLength);
    if (rc == deviceIDLength)
        return FlashStatus_Success;

    if (rc == 0)
    {
        LOG_INF("FlashUtility_SaveDeviceID data already saved");
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_SaveDeviceID failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Loads the device ID from flash.
 *
 * @param[in] deviceID           The device ID to save
 * @param[in] deviceIDLength     The length of the device ID
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadDeviceID(char *deviceID, uint8_t deviceIDLength)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_LoadDeviceID not initialized");
        return FlashStatus_Failure;
    }
    int rc = nvs_read(&fs, DEVICE_ID_FS_ID, deviceID, deviceIDLength);
    if (rc == deviceIDLength)
        return FlashStatus_Success;

    LOG_ERR("FlashUtility_LoadDeviceID failed, err %d", rc);
    return FlashStatus_Failure;
}

/**
 * @brief Saves half_cycles to memory.
 *
 * @param[in] half_cycles        The variable to save
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveHalfCycles(uint32_t half_cycles)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_SaveHalfCycles not initialized");
        return FlashStatus_Failure;
    }
    static uint32_t last_half_cycles = 0;
    LOG_INF("FlashUtility_SaveHalfCycles %d", half_cycles);

    if (half_cycles == last_half_cycles)
    {
        LOG_INF("FlashUtility_SaveHalfCycles no change");
        return FlashStatus_Success;
    }

    int rc = nvs_write(&fs, HLF_CYCLE_ID, &half_cycles, sizeof(half_cycles));
    if (rc == 0)
    {
        LOG_INF("FlashUtility_SaveHalfCycles half cycles already saved");
        return FlashStatus_Success;
    }

    if (rc == sizeof(half_cycles))
    {
        last_half_cycles = half_cycles;
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_SaveHalfCycles failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Loads total pulses from flash.
 *
 * @param[in] half_cycles        Variable to load
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadHalfCycles(uint32_t *half_cycles)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_LoadHalfCycles not initialized");
        return FlashStatus_Failure;
    }

    int rc = nvs_read(&fs, HLF_CYCLE_ID, half_cycles, sizeof(half_cycles));
    if (rc == -ENOENT)
    {
        LOG_WRN("FlashUtility_LoadHalfCycles no data");
        return FlashStatus_NoData;
    }
    else if (rc == sizeof(half_cycles))
    {
        LOG_INF("FlashUtility_LoadHalfCycles %u", *half_cycles);
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_LoadHalfCycles failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Saves LowestFlowOverPeriodTracker Data
 *
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_Save_LowestFlowOverPeriodTrackerData(uint16_t numberBuckets, uint16_t bucketDuration)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_Save_LowestFlowOverPeriodTrackerData not initialized");
        return FlashStatus_Failure;
    }
    LOG_INF("FlashUtility_Save_LowestFlowOverPeriodTrackerData, numberBuckets %d, bucketDuration %d", numberBuckets, bucketDuration);

    uint16_t data[2] = {numberBuckets, bucketDuration};
    int rc = nvs_write(&fs, LFOPTD_FS_ID, data, sizeof(data));
    if (rc == 0)
    {
        LOG_INF("FlashUtility_Save_LowestFlowOverPeriodTrackerData data already saved");
        return FlashStatus_Success;
    }
    else if (rc == sizeof(data))
    {
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_Save_LowestFlowOverPeriodTrackerData failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Loads LowestFlowOverPeriodTracker Data
 *
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_Load_LowestFlowOverPeriodTrackerData(uint16_t *numberBuckets, uint16_t *bucketDuration)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_Load_LowestFlowOverPeriodTrackerData not initialized");
        return FlashStatus_Failure;
    }
    uint16_t data[2] = {0};

    int rc = nvs_read(&fs, LFOPTD_FS_ID, data, sizeof(data));
    if (rc == -ENOENT)
    {
        LOG_WRN("FlashUtility_Load_LowestFlowOverPeriodTrackerData no data, defaulting to 60, 1");
        *numberBuckets = 60;
        *bucketDuration = 1;
        return FlashStatus_NoData;
    }
    else if (rc == sizeof(data))
    {
        *numberBuckets = data[0];
        *bucketDuration = data[1];

        LOG_INF("FlashUtility_Load_LowestFlowOverPeriodTrackerData, numberBuckets %d, bucketDuration %d", *numberBuckets, *bucketDuration);

        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_Load_LowestFlowOverPeriodTrackerData failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Erases all lowest flow over period tracker data.
 **/
FlashStatus_t FlashUtility_Erase_LowestFlowOverPeriodTrackerData(void)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_Erase_LowestFlowOverPeriodTrackerData not initialized");
        return FlashStatus_Failure;
    }

    int rc = nvs_delete(&fs, LFOPTD_FS_ID);
    if (!rc)
        return FlashStatus_Success;

    LOG_ERR("FlashUtility_Erase_LowestFlowOverPeriodTrackerData failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Erases the configuration to flash.
 *
 * @retval FlashStatus_t of the operation
 * @note This function will erase the configurations section from the flash.
 *
 **/

FlashStatus_t FlashUtility_EraseConfiguration(void)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_EraseConfiguration not initialized");
        return FlashStatus_Failure;
    }
    int rc = nvs_delete(&fs, CONFIGURATION_FS_ID);
    if (!rc)
        return FlashStatus_Success;

    LOG_ERR("FlashUtility_EraseConfiguration failed, err %d", rc);

    return FlashStatus_Failure;
}

/**
 * @brief Loads configuration
 *
 * @retval FlashStatus_t of the operation
 * @note This function will erase the entire flash page before writing the data.
 *      This is done to ensure that the data is written correctly.
 * @note Currently these variables are all globals, which is why there is no parameters
 **/
FlashStatus_t FlashUtility_LoadConfiguration(void)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_LoadConfiguration not initialized");
        return FlashStatus_Failure;
    }

    /*
        #define VERSION_HEADER 0x00000001 //todo

        uint32_t l_variables[32]; //load variables
        int rc = nvs_read(&fs, CONFIGURATION_FS_ID, l_variables, 13); // TODO: verify the l_variables count
        if(rc != (13*sizeof(uint32_t))) return FlashStatus_Failure;

        uint8_t loadAddr = 0;
        uint32_t header = l_variables[loadAddr++];

        LOG_INF("FU - Load config header=%d", header);

        if (header == 0)
        {
            return FlashStatus_NoData;
        }

        if (VERSION_HEADER != header)
        {
            LOG_ERR("Versions do not match up, deleting. version=%d", VERSION_HEADER);
            FlashUtility_EraseConfiguration();
            return FlashStatus_Failure;
        }
        uint32_t length = l_variables[loadAddr++];

        LOG_INF("Length %d Loaded: ", length);
        uint32_t r_variable[MAX_VARIABLE_LENGTH];
        for (uint8_t i = 0; i < length; i++)
        {
            r_variable[i] =  l_variables[loadAddr++];
        }

        Algorithm.Set_Variables(r_variable, length);
        LOG_INF("");
    */
    // error: 'ALGORITHM_t' has no member named 'Set_Variables'

    return FlashStatus_Success;
}

/**
 * @brief Saves the configuration to flash.
 *
 * @retval FlashStatus_t of the operation
 * @note This function will erase the entire flash page before writing the data.
 *      This is done to ensure that the data is written correctly.
 * @note Currently these variables are all globals, which is why there is no parameters
 **/

FlashStatus_t FlashUtility_SaveConfiguration(void)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_SaveConfiguration not initialized");
        return FlashStatus_Failure;
    }

    /*
        FlashUtility_EraseConfiguration();

        uint8_t variables_count = 0; // leave room for header
        uint32_t s_variables[32]; //store variables
        uint32_t header = VERSION_HEADER;
        s_variables[variables_count++] = header;
        LOG_INF("FU Save header: %u ", header);
        uint32_t variables[MAX_VARIABLE_LENGTH] = {0};
        uint8_t length;
        Algorithm.Get_Variables(variables, &length);
        LOG_INF("length %u ", length);
        s_variables[variables_count++] = length;
        for (uint8_t i = 0; i < length; i++)
        {
            s_variables[variables_count++] = variables[i];
            LOG_INF("%u ", variables[i]);
        }
        LOG_INF("");

        LOG_INF("FU - Saved algorithm config head=%X\r", header);

        int rc = nvs_write(&fs, CONFIGURATION_FS_ID, s_variables, variables_count);
        if(rc == (variables_count*sizeof(uint32_t))) return FlashStatus_Success;

    */
    // error: 'ALGORITHM_t' has no member named 'Get_Variables'

    return FlashStatus_Failure;
}

/**
 * @brief Saves the provisioned state to flash.
 *
 * @retval FlashStatus_t of the operation
 * @note This function will erase the entire flash page before writing the data.
 *      This is done to ensure that the data is written correctly.
 * @note Currently these variables are all globals, which is why there is no parameters
 **/

FlashStatus_t FlashUtility_SetProvisioning_State(bool provisioned)
{
    LOG_INF("FU - Setting ptovisioning state %d", provisioned);

    uint8_t __provisioned = provisioned ? 1 : 0;
    int rc = nvs_write(&fs, PROVISIONING_STATE_ID, &__provisioned, sizeof(__provisioned));
    if (rc == sizeof(__provisioned))
        return FlashStatus_Success;

    if (rc == 0)
    {
        LOG_INF("FlashUtility_SetProvisioning_State data already saved");
        return FlashStatus_Success;
    }

    return FlashStatus_Failure;
}

/**
 * @brief Loads the provisioning state.
 *
 * @param[in] provisioned        Variable to load
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_GetProvisioning_State(bool *provisioned)
{
    uint8_t __provisioned = 0;

    int rc = nvs_read(&fs, PROVISIONING_STATE_ID, &__provisioned, sizeof(__provisioned));

    if (rc == -ENOENT)
    {
        LOG_WRN("FlashUtility_GetProvisioning_State no data, defaulting to false");
        *provisioned = false;
        return FlashStatus_NoData;
    }
    else if (rc == sizeof(__provisioned))
    {
        LOG_INF("FlashUtility_GetProvisioning_State %u", __provisioned);
        *provisioned = __provisioned ? true : false;
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_GetProvisioning_State failed, err %d", rc);
    *provisioned = false;
    return FlashStatus_Failure;
}

/**
 * @brief Saves the device state to flash.
 *
 * @param[in] is_enabled    The device state to save (true for enabled, false for disabled)
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveDeviceState(bool is_enabled)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_SaveDeviceState not initialized");
        return FlashStatus_Failure;
    }

    uint8_t state = is_enabled ? 1 : 0;
    int rc = nvs_write(&fs, DEVICE_STATE_ID, &state, sizeof(state));
    if (rc == sizeof(state))
    {
        LOG_INF("Device state saved: %s", is_enabled ? "ON" : "OFF");
        return FlashStatus_Success;
    }

    if (rc == 0)
    {
        LOG_INF("FlashUtility_SaveDeviceState data already saved");
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_SaveDeviceState failed, err %d", rc);
    return FlashStatus_Failure;
}

/**
 * @brief Loads the device state from flash.
 *
 * @param[out] is_enabled   Pointer to store the loaded device state
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadDeviceState(bool *is_enabled)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_LoadDeviceState not initialized");
        return FlashStatus_Failure;
    }

    uint8_t state = 1; // Default to enabled if no data is found
    int rc = nvs_read(&fs, DEVICE_STATE_ID, &state, sizeof(state));
    if (rc == 0)
    {
        LOG_WRN("No device state found, using default: ON");
        *is_enabled = true;
        return FlashStatus_NoData;
    }
    else if (rc == sizeof(state))
    {
        *is_enabled = (state == 1);
        LOG_INF("Device state loaded: %s", *is_enabled ? "ON" : "OFF");
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_LoadDeviceState failed, err %d", rc);
    *is_enabled = true; // Default to enabled on error
    return FlashStatus_Failure;
}

/**
 * @brief Saves the device type configuration to flash.
 *
 * @param[in] device_type    The device type to save (enum value)
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveDeviceType(device_type_t device_type)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_SaveDeviceType not initialized");
        return FlashStatus_Failure;
    }

    // Validate enum value
    if (device_type > DEVICE_TYPE_SENSUS_PROTOCOL)
    {
        LOG_ERR("Invalid device type: %d", device_type);
        return FlashStatus_Failure;
    }

    uint8_t type = (uint8_t)device_type;
    int rc = nvs_write(&fs, DEVICE_TYPE_ID, &type, sizeof(type));
    if (rc == sizeof(type))
    {
        const char *type_names[] = {"Magnetometer", "Pulse Tracker", "Sensus Protocol"};
        LOG_INF("Device type saved: %s (%d)", type_names[device_type], device_type);
        return FlashStatus_Success;
    }

    if (rc == 0)
    {
        LOG_INF("FlashUtility_SaveDeviceType data already saved");
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_SaveDeviceType failed, err %d", rc);
    return FlashStatus_Failure;
}

/**
 * @brief Loads the device type configuration from flash.
 *
 * @param[out] device_type   Pointer to store the loaded device type
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadDeviceType(device_type_t *device_type)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_LoadDeviceType not initialized");
        return FlashStatus_Failure;
    }

    uint8_t type = 0; // Default to magnetometer if no data is found
    int rc = nvs_read(&fs, DEVICE_TYPE_ID, &type, sizeof(type));
    if (rc == 0 || rc == -ENOENT)
    {
        LOG_WRN("No device type found, using default: Magnetometer %d", rc);
        *device_type = DEVICE_TYPE_MAGNETOMETER;
        return FlashStatus_NoData;
    }
    else if (rc == sizeof(type))
    {
        // Validate the loaded value
        if (type > DEVICE_TYPE_SENSUS_PROTOCOL)
        {
            LOG_WRN("Invalid device type value %d, using default: Magnetometer", type);
            *device_type = DEVICE_TYPE_MAGNETOMETER;
            return FlashStatus_NoData;
        }

        *device_type = (device_type_t)type;
        const char *type_names[] = {"Magnetometer", "Pulse Tracker", "Sensus Protocol"};
        LOG_INF("Device type loaded: %s (%d)", type_names[*device_type], *device_type);
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_LoadDeviceType failed, err %d", rc);
    *device_type = DEVICE_TYPE_MAGNETOMETER; // Default to magnetometer on error
    return FlashStatus_Failure;
}

/**
 * @brief Saves the algorithm logging state to flash.
 *
 * @param[in] logging_enabled    The logging state to save (true for enabled, false for disabled)
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_SaveLoggingState(bool logging_enabled)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_SaveLoggingState not initialized");
        return FlashStatus_Failure;
    }

    uint8_t state = logging_enabled ? 1 : 0;
    int rc = nvs_write(&fs, LOGGING_STATE_ID, &state, sizeof(state));
    if (rc == sizeof(state))
    {
        LOG_INF("Logging state saved: %s", logging_enabled ? "ENABLED" : "DISABLED");
        return FlashStatus_Success;
    }

    if (rc == 0)
    {
        LOG_INF("FlashUtility_SaveLoggingState data already saved");
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_SaveLoggingState failed, err %d", rc);
    return FlashStatus_Failure;
}

/**
 * @brief Loads the algorithm logging state from flash.
 *
 * @param[out] logging_enabled   Pointer to store the loaded logging state
 * @retval FlashStatus_t of the operation
 **/
FlashStatus_t FlashUtility_LoadLoggingState(bool *logging_enabled)
{
    if (!flash_initialized)
    {
        LOG_ERR("FlashUtility_LoadLoggingState not initialized");
        return FlashStatus_Failure;
    }

    uint8_t state = 1; // Default to enabled if no data is found
    int rc = nvs_read(&fs, LOGGING_STATE_ID, &state, sizeof(state));
    if (rc == 0 || rc == -ENOENT)
    {
        LOG_WRN("No logging state found, using default: ENABLED %d", rc);
        *logging_enabled = true;
        return FlashStatus_NoData;
    }
    else if (rc == sizeof(state))
    {
        *logging_enabled = (state == 1);
        LOG_INF("Logging state loaded: %s", *logging_enabled ? "ENABLED" : "DISABLED");
        return FlashStatus_Success;
    }

    LOG_ERR("FlashUtility_LoadLoggingState failed, err %d", rc);
    *logging_enabled = true; // Default to enabled on error
    return FlashStatus_Failure;
}

/************************ (C) COPYRIGHT NOWi Sensors LLC *****END OF FILE****/
