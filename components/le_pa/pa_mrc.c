/** @file pa_mrc.c
 *
 * AT implementation of c_pa_mrc API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_mrc.h"
#include "pa_utils.h"
#include "pa_mrc_local.h"

#ifdef LTE_ONLY_TARGET
//--------------------------------------------------------------------------------------------------
/**
 * Unsolicited +CEREG references
 */
//--------------------------------------------------------------------------------------------------
static const char RegisterUnsolicited[] = "+CEREG";
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Check if the current device RAT is configured in GSM
 *
 * @return
        - true      If currently configured in GSM mode
        - false     Otherwise
 */
//--------------------------------------------------------------------------------------------------
bool pa_mrc_local_IsGSMMode
(
    void
)
{
#ifdef LTE_ONLY_TARGET
    return false;
#else
    return true;
#endif
}
//--------------------------------------------------------------------------------------------------
/**
 * This function configures the Network registration setting.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_ConfigureNetworkReg
(
    pa_mrc_NetworkRegSetting_t  setting ///< [IN] The selected Network registration setting.
)
{
    char                 command[LE_ATDEFS_COMMAND_MAX_BYTES];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

#ifdef LTE_ONLY_TARGET
    snprintf(command,LE_ATDEFS_COMMAND_MAX_BYTES,"AT+CEREG=%d", setting);
#else
    snprintf(command,LE_ATDEFS_COMMAND_MAX_BYTES,"AT+CREG=%d", setting);
#endif

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        command,
                                        DEFAULT_EMPTY_INTERMEDIATE,
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res == LE_OK)
    {
        le_atClient_Delete(cmdRef);
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the Signal Strength information.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER Bad parameter passed to the function
 * @return LE_OUT_OF_RANGE  The signal strength values are not known or not detectable.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetSignalStrength
(
    int32_t*          rssiPtr    ///< [OUT] The received signal strength (in dBm).
)
{
    le_atClient_CmdRef_t cmdRef   = NULL;
    le_result_t          res      = LE_FAULT;
    char*                tokenPtr = NULL;
    char*                rest     = NULL;
    char*                savePtr  = NULL;
    int32_t              val      = 0;
    char intermediateResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];
    char finalResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];

    if (!rssiPtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CSQ",
                                        "+CSQ:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                        finalResponse,
                                        LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(finalResponse,"OK") != 0)
    {
        LE_ERROR("Final response not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                                                   intermediateResponse,
                                                   LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
    }
    else
    {
        rest     = intermediateResponse+strlen("+CSQ: ");
        tokenPtr = strtok_r(rest, ",", &savePtr);

        if (tokenPtr == NULL)
        {
            LE_ERROR("Failed to get QOS");
            return LE_FAULT;
        }

        val      = atoi(tokenPtr);

        if (val == 99)
        {
            LE_WARN("Quality signal not detectable");
            res = LE_OUT_OF_RANGE;
        }
        else
        {
            *rssiPtr = (-113+(2*val));
            res = LE_OK;
        }
    }
    le_atClient_Delete(cmdRef);
    return res;

}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the number of preferred operators present in the list.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_CountPreferredOperators
(
    bool      plmnStatic,   ///< [IN] Include Static preferred Operators.
    bool      plmnUser,     ///< [IN] Include Users preferred Operators.
    int32_t*  nbItemPtr     ///< [OUT] number of Preferred operator found if success.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the current preferred operators.
 *
 * @return
 *      - LE_OK on success
 *      - LE_NOT_FOUND if Preferred operator list is not available
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetPreferredOperators
(
    pa_mrc_PreferredNetworkOperator_t*   preferredOperatorPtr,
                          ///< [IN/OUT] The preferred operators pointer.
    bool     plmnStatic,  ///< [IN] Include Static preferred Operators.
    bool     plmnUser,    ///< [IN] Include Users preferred Operators.
    int32_t* nbItemPtr    ///< [IN/OUT] number of Preferred operator to find (in) and written (out).
)
{
    return LE_NOT_FOUND;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to apply the preferred list into the modem
 *
 * @return
 *      - LE_OK             on success
 *      - LE_FAULT          for all other errors
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SavePreferredOperators
(
    le_dls_List_t* preferredOperatorsListPtr ///< [IN] List of preferred network operator
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the base station identity Code (BSIC) for the serving cell on GSM network.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER Bad parameter passed to the function
 * @return LE_FAULT         The function failed.
 * @return LE_UNAVAILABLE   The BSIC is not available. The Bsic value is set to UINT8_MAX.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetServingCellGsmBsic
(
    uint8_t* bsicPtr    ///< [OUT] The Bsic value
)
{
    if (!bsicPtr)
    {
        LE_ERROR("bsicPtr is NULL");
        return LE_BAD_PARAMETER;
    }

    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the serving cell primary scrambling code.
 *
 * @return The serving cell primary scrambling code. UINT16_MAX value is returned if the value is
 * not available.
 */
//--------------------------------------------------------------------------------------------------
uint16_t pa_mrc_GetServingCellScramblingCode
(
    void
)
{
    return UINT16_MAX;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the Radio Access Technology.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed to get the Signal strength information.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetRadioAccessTechInUse
(
    le_mrc_Rat_t*   ratPtr    ///< [OUT] The Radio Access Technology.
)
{
    le_atClient_CmdRef_t cmdRef  = NULL;
    le_result_t          res     = LE_FAULT;
    char*                bandPtr = NULL;
    int                  bitMask = 0;
    char                 intermediateResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];
    char                 finalResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+KBND?",
                                        "+KBND:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       finalResponse,
                                       LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(finalResponse,"OK") != 0)
    {
        LE_ERROR("Final response not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                                                   intermediateResponse,
                                                   LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
    }
    else
    {
        bandPtr = intermediateResponse+strlen("+KBND: ");
        bitMask = atoi(bandPtr);

        if ((bitMask >= 1) && (bitMask <= 8))
        {
            *ratPtr = LE_MRC_RAT_GSM;
        }
        else if ((bitMask >= 10) && (bitMask <= 200))
        {
            *ratPtr = LE_MRC_RAT_UMTS;
        }
        else
        {
            *ratPtr = LE_MRC_RAT_UNKNOWN;
        }
    }
    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function retrieves the Neighboring Cells information.
 * Each cell information is queued in the list specified with the IN/OUT parameter.
 * Neither add nor remove of elements in the list can be done outside this function.
 *
 * @return LE_FAULT          The function failed to retrieve the Neighboring Cells information.
 * @return a positive value  The function succeeded. The number of cells which the information have
 *                           been retrieved.
 */
//--------------------------------------------------------------------------------------------------
int32_t pa_mrc_GetNeighborCellsInfo
(
    le_dls_List_t* cellInfoListPtr  ///< [OUT] The Neighboring Cells information.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function measures the Signal metrics.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_MeasureSignalMetrics
(
    pa_mrc_SignalMetrics_t* metricsPtr    ///< [OUT] The signal metrics.
)
{
    le_mrc_Rat_t  rat;
    int32_t       signal;

    metricsPtr->rat = pa_mrc_GetRadioAccessTechInUse(&rat);
    metricsPtr->ss  = pa_mrc_GetSignalStrength(&signal);
    metricsPtr->er  = 0xFFFFFFFF;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Packet Switched state.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetPacketSwitchedState
(
    le_mrc_NetRegState_t* statePtr  ///< [OUT] The current Packet switched state.
)
{
    LE_WARN("Unsupported function called");
    return LE_FAULT;
}

// TODO:To be cleaned.
#ifdef LTE_ONLY_TARGET
//--------------------------------------------------------------------------------------------------
/**
* This function must be called to get the unsolicited string to subscribe
*
* @return The unsolicited string
*/
//--------------------------------------------------------------------------------------------------
const char * pa_mrc_local_GetRegisterUnso
(
  void
)
{
    return RegisterUnsolicited;
}
#endif

//--------------------------------------------------------------------------------------------------
/**
 * This function set the network operator mode (text or number mode).
 *
 * @return LE_OK            The function succeeded.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_local_SetOperatorTextMode
(
    bool    textMode
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;
    char                 responseStr[PA_AT_LOCAL_SHORT_SIZE];

    if (textMode)
    {
        res = le_atClient_SetCommandAndSend(&cmdRef,
                                            pa_utils_GetAtDeviceRef(),
                                            "AT+COPS=3,0",
                                            DEFAULT_EMPTY_INTERMEDIATE,
                                            DEFAULT_AT_RESPONSE,
                                            DEFAULT_AT_CMD_TIMEOUT);
    }
    else
    {
        res = le_atClient_SetCommandAndSend(&cmdRef,
                                            pa_utils_GetAtDeviceRef(),
                                            "AT+COPS=3,2",
                                            DEFAULT_EMPTY_INTERMEDIATE,
                                            DEFAULT_AT_RESPONSE,
                                            DEFAULT_AT_CMD_TIMEOUT);
    }

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,responseStr,sizeof(responseStr));
    le_atClient_Delete(cmdRef);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
    }
    else if (strcmp(responseStr,"OK") != 0)
    {
        LE_ERROR("Final response not OK");
        return LE_FAULT;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the CREG mode
 *
 */
//--------------------------------------------------------------------------------------------------
void  pa_mrc_local_SetCregMode
(
    int32_t cregMode
)
{
    char localBuffStr[PA_AT_LOCAL_SHORT_SIZE];
    snprintf(localBuffStr, sizeof(localBuffStr), "AT+CREG=%" PRIi32, cregMode);

    pa_utils_GetATIntermediateResponse(localBuffStr, DEFAULT_EMPTY_INTERMEDIATE,
        localBuffStr, sizeof(localBuffStr));
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the CEREG mode
 *
 */
//--------------------------------------------------------------------------------------------------
void  pa_mrc_local_SetCeregMode
(
    int32_t ceregMode
)
{
    char localBuffStr[PA_AT_LOCAL_SHORT_SIZE];
    snprintf(localBuffStr, sizeof(localBuffStr), "AT+CEREG=%" PRIi32, ceregMode);

    pa_utils_GetATIntermediateResponse(localBuffStr, DEFAULT_EMPTY_INTERMEDIATE,
        localBuffStr, sizeof(localBuffStr));
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the serving Cell Timing Advance index value. Timing advance index value is in the range
 * from 0 to 1280.
 *
 * @return The serving Cell timing advance index value. UINT32_MAX value is returned if the value
 * is not available.
 *
 * @note API is not supported on all platforms.
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_mrc_GetServingCellTimingAdvance
(
    void
)
{
    return UINT32_MAX;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the serving cell Radio Frequency Channel Number. The EARFCN is in the range from 0 to 262143.
 *
 * @return The serving Cell frequency channel number. UINT32_MAX value is returned if the value is
 * not available.
 *
 * @note API is not supported on all platforms.
 * @note <b>multi-app safe</b>
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_mrc_GetServingCellEarfcn
(
    void
)
{
    return UINT32_MAX;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the Physical serving Cell Id. The Physical cell Id is in the range from 0 to 503.
 *
 * @return The Physical serving Cell Id. UINT16_MAX value is returned if the value is
 * not available.
 *
 * @note API is not supported on all platforms.
 * @note <b>multi-app safe</b>
 */
//--------------------------------------------------------------------------------------------------
uint16_t pa_mrc_GetPhysicalServingLteCellId
(
    void
)
{
    return UINT16_MAX;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function retrieves the network time from the modem.
 *
 * @return
 * - LE_FAULT         The function failed to get the value.
 * - LE_UNAVAILABLE   No valid user time was returned.
 * - LE_UNSUPPORTED   The feature is not supported.
 * - LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SyncNetworkTime
(
    void
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler to report a Network Time event.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_AddNetworkTimeIndHandler
(
    pa_mrc_NetworkTimeHandlerFunc_t networkTimeIndHandler       ///< [IN] The handler function
                                                                ///  to handle network
                                                                ///  time indication.
)
{
    LE_ASSERT(networkTimeIndHandler != NULL);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to delete the list of pci Scan Information
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_DeletePciScanInformation
(
    le_dls_List_t *scanInformationListPtr ///< [IN] list of pa_mrc_ScanInformation_t
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to delete the list of Plmn Information
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_DeletePlmnScanInformation
(
    le_dls_List_t *scanInformationListPtr ///< [IN] list of pa_mrc_PlmnInformation_t
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Enable or disable monitoring on Rank change indicate . By default, monitoring is disabled.
 *
 * @return
 * - LE_FAULT         The function failed to set RankChange monitoring.
 * - LE_OK            The function succeeded.
 * - LE_UNSUPPORTED   The feature is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetRankChangeMonitoring
(
    bool activation    ///< [IN] Indication activation request
)
{

    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Rank Indicator change handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_AddRankChangeHandler
(
    pa_mrc_RankChangeHdlrFunc_t handlerFuncPtr ///< [IN] The handler function.
)
{
    LE_ERROR("Unsupported function called");
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for Rank Indicator change
 * handling.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_RemoveRankChangeHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    LE_ERROR("Unsupported function called");
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the Radio Band currently in use.
 *
 * @return
 * - LE_OK              On success
 * - LE_FAULT           On failure
 * - LE_UNSUPPORTED  The feature is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetRadioBandInUse
(
    uint32_t*   bandPtr    ///< [OUT] The Radio Band.
)
{
    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieves the LTE Data modulation and coding scheme for the physical multicast channel.
 *
 * @return
 *  - LE_OK             The function succeeded.
 *  - LE_FAULT          The function failed.
 *  - LE_UNSUPPORTED    The feature is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetLteEmbmsInfo
(
    uint8_t* mcs
)
{
    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieves the detailed TX power information.
 *
 * @return
 *  - LE_OK             The function succeeded.
 *  - LE_FAULT          The function failed.
 *  - LE_UNSUPPORTED    The feature is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetTxPowerInfo
(
    int32_t* txPwr
)
{
    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This command gets LTE CQI
 *
 * @return
 *  - LE_OK             The function succeeded.
 *  - LE_FAULT          The function failed.
 *  - LE_UNSUPPORTED    The feature is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetLteCqi
(
    uint32_t* cqi
)
{
    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
}
