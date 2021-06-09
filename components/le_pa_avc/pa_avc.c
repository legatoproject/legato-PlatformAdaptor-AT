//--------------------------------------------------------------------------------------------------
/*
 * Empty component for now
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "pa_avc.h"

//--------------------------------------------------------------------------------------------------
/**
 * Number of seconds in a minute
 */
//--------------------------------------------------------------------------------------------------
#define SECONDS_IN_A_MIN 60

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the EDM polling timer to a value in seconds
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 *      - LE_FAULT upon failure to set it.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_avc_SetEdmPollingTimerInSeconds
(
    uint32_t pollingTimeSecs ///< [IN] Polling timer interval, seconds
)
{
    le_result_t result = LE_OK;
    le_atClient_DeviceRef_t devRef;
    uint32_t pollingTimeMins = pollingTimeSecs / SECONDS_IN_A_MIN;

    LE_INFO("Setting EDM polling timer to %u minutes", pollingTimeMins);

    // Prepare the device
    int fd = open("/dev/ttyAT", O_RDWR | O_NOCTTY | O_NONBLOCK);
    devRef = le_atClient_Start(fd);
    le_atClient_CmdRef_t cmdRef;
    cmdRef = le_atClient_Create();
    result = le_atClient_SetDevice(cmdRef, devRef);
    if (result != LE_OK)
    {
        LE_ERROR("Error setting AT client device: %s", LE_RESULT_TXT(result));
        goto exit;
    }

    // Send the command: AT+DRCC=0,T where T is periodic session time in minutes.
    char cmdBuf[64] = {0};
    snprintf(cmdBuf, sizeof(cmdBuf), "AT+DRCC=0,%u", (unsigned int)pollingTimeMins);
    LE_INFO("Sending AT command: %s", cmdBuf);
    result = le_atClient_SetCommandAndSend(&cmdRef,
                                           devRef,
                                           cmdBuf,
                                           "",
                                           "OK|ERROR|+CME ERROR",
                                           5000);
    if (result != LE_OK)
    {
        /* A timeout is a result of the modem not supporting this command. We will
         * proceed without returning an error */
        if (result == LE_TIMEOUT)
        {
            LE_DEBUG("AT cmd timed out. Command not supported by modem.");
            result = LE_OK;
        }
        /* A fault can occur when modem does not have an updated support for DRCC=0,T where
        *  0 parameter was used to set server configuration. */
        else if ((result == LE_FAULT) && (pollingTimeMins > 2))
        {
            LE_DEBUG("AT cmd faulted with invalid range values. Command not fully supported.");
            result = LE_OK;
        }
        else
        {
            LE_ERROR("Error sending AT command: %s", LE_RESULT_TXT(result));
        }
        cmdRef = NULL;
        goto exit;
    }

    // Get the response
    char respBuf[LE_ATDEFS_RESPONSE_MAX_BYTES] = {0};
    result = le_atClient_GetFinalResponse(cmdRef,respBuf, LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (result != LE_OK)
    {
        LE_ERROR("Error getting AT command response: %s", LE_RESULT_TXT(result));
        goto exit;
    }
    else if (strcmp(respBuf,"OK") != 0)
    {
        LE_ERROR("Final response not OK: '%s'", respBuf);
        result = LE_FAULT;
        goto exit;
    }
    LE_DEBUG("Response: %s value %s", LE_RESULT_TXT(result), respBuf);

    // Close/cleanup
exit:
    if (cmdRef)
    {
        result = le_atClient_Delete(cmdRef);
        if (result != LE_OK)
        {
            LE_ERROR("Error deleting AT client: %s", LE_RESULT_TXT(result));
        }
    }
    close(fd);
    return result;
}

COMPONENT_INIT
{
}
