/** @file pa_sim.c
 *
 * AT implementation of c_pa_sim API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */


#include "legato.h"
#include "interfaces.h"

#include "pa_sim.h"
#include "pa_utils.h"
#include "pa_sim_utils_local.h"


//--------------------------------------------------------------------------------------------------
/**
 * Sim memory pool size
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_SIMEVENT_POOL_SIZE  1

//--------------------------------------------------------------------------------------------------
/**
 * Define static SIM memory pool
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(SimEventPool, DEFAULT_SIMEVENT_POOL_SIZE, sizeof(pa_sim_Event_t));


//--------------------------------------------------------------------------------------------------
/**
 * Sim memory pool
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t   SimEventPoolRef;

//--------------------------------------------------------------------------------------------------
/**
 * Unsolicited event
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t      EventUnsolicitedId;

//--------------------------------------------------------------------------------------------------
/**
 * Sim Status event
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t      EventNewSimStateId;

//--------------------------------------------------------------------------------------------------
/**
 * External SIM selected by default
 */
//--------------------------------------------------------------------------------------------------
static le_sim_Id_t        UimSelect = LE_SIM_EXTERNAL_SLOT_1;


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to send event to all registered handler
 *
 */
//--------------------------------------------------------------------------------------------------
static void ReportState
(
    uint32_t        simCard,  ///< [IN] Sim Card Number
    le_sim_States_t simState  ///< [IN] Sim Card Status
)
{
    pa_sim_Event_t* eventPtr = le_mem_ForceAlloc(SimEventPoolRef);
    eventPtr->simId = simCard;
    eventPtr->state = simState;

    LE_DEBUG("Send Event SIM identifier %d, SIM state %d",eventPtr->simId,eventPtr->state);
    le_event_ReportWithRefCounting(EventNewSimStateId,eventPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for SIM state changing handling.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SimUnsolicitedHandler
(
    void* reportPtr
)
{
    char*           unsolPtr = reportPtr;
    le_sim_States_t simState = LE_SIM_STATE_UNKNOWN;

    if (pa_sim_utils_CheckStatus(unsolPtr,&simState))
    {
        ReportState(UimSelect,simState);
    }
}

//--------------------------------------------------------------------------------------------------
// APIs.
//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the sim module.
 *
 * @return LE_FAULT         The function failed to initialize the module.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_Init
(
    void
)
{
    SimEventPoolRef = le_mem_InitStaticPool(SimEventPool,
                                            DEFAULT_SIMEVENT_POOL_SIZE,
                                            sizeof(pa_sim_Event_t));

    EventUnsolicitedId = le_event_CreateId("SIMEventIdUnsol", LE_ATDEFS_UNSOLICITED_MAX_BYTES);
    EventNewSimStateId = le_event_CreateIdWithRefCounting("SIMEventIdNewState");
    le_event_AddHandler("SimUnsolicitedHandler", EventUnsolicitedId, SimUnsolicitedHandler);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function counts number of sim card available.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return number           number of count slots
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_sim_CountSlots
(
    void
)
{
     return 1;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function selects the Card on which all further SIM operations have to be operated.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_SelectCard
(
    le_sim_Id_t  cardId     ///< The SIM to be selected
)
{
    if (cardId != LE_SIM_EXTERNAL_SLOT_1)
    {
        return LE_FAULT;
    }
    UimSelect = cardId;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets sim selection mode.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetSimMode
(
    le_sim_SimMode_t*  simModePtr     ///< [OUT] The SIM selection mode
)
{
    *simModePtr = LE_SIM_FORCE_EXTERNAL;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function get the card on which operations are operated.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetSelectedCard
(
    le_sim_Id_t*  cardIdPtr     ///< [OUT] The card number selected.
)
{
   *cardIdPtr = UimSelect;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function get the card identification (ICCID).
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetCardIdentification
(
    pa_sim_CardId_t iccid     ///< [OUT] CCID value
)
{
    le_atClient_CmdRef_t cmdRef   = NULL;
    char*                tokenPtr = NULL;
    le_result_t          res      = LE_OK;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE];

    if (!iccid)
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CCID",
                                        "+CCID:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_STRING_SIZE);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(responseStr,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_STRING_SIZE);
    le_atClient_Delete(cmdRef);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        return res;
    }

    // Cut the string for keep just the CCID number
    tokenPtr = strtok(responseStr,"+CCID: ");
    if(tokenPtr)
    {
        strncpy(iccid, tokenPtr, strlen(tokenPtr));
    }
    else
    {
        res= LE_FAULT;
    }

    return res;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function get the International Mobile Subscriber Identity (IMSI).
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetIMSI
(
    pa_sim_Imsi_t imsi   ///< [OUT] IMSI value
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE];

    if (!imsi)
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CIMI",
                                        "0|1|2|3|4|5|6|7|8|9",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        sizeof(responseStr));

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(responseStr,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
        responseStr,
        sizeof(responseStr));
    le_atClient_Delete(cmdRef);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        return res;
    }

    strncpy(imsi, responseStr, sizeof(pa_sim_Imsi_t));
    return res;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function get the SIM Status.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetState
(
    le_sim_States_t* statePtr    ///< [OUT] SIM state
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};

    if (!statePtr)
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

   *statePtr = LE_SIM_STATE_UNKNOWN;

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CPIN?",
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        sizeof(responseStr));

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(responseStr,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }


    if (pa_sim_utils_CheckStatus(responseStr,statePtr))
    {
        ReportState(UimSelect,*statePtr);
    }

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for new SIM state notification handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_sim_AddNewStateHandler
(
    pa_sim_NewStateHdlrFunc_t handler ///< [IN] The handler function.
)
{
    LE_DEBUG("Set new SIM State handler");

    LE_FATAL_IF(handler == NULL, "New SIM State handler is NULL");

    return (le_event_AddHandler("NewSIMStateHandler",
                                EventNewSimStateId,
                                (le_event_HandlerFunc_t) handler));
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for new SIM state notification handling.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_RemoveNewStateHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    le_event_RemoveHandler(handlerRef);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function enter the PIN code.
 *
 * @return LE_FAULT         The function failed to enter the value.
 * @return LE_TIMEOUT       No response was received from the SIM card.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_EnterPIN
(
    pa_sim_PinType_t   type,  ///< [IN] pin type
    const pa_sim_Pin_t pin    ///< [IN] pin code
)
{
    char                 command[PA_AT_LOCAL_SHORT_SIZE];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;

    snprintf(command,PA_AT_LOCAL_SHORT_SIZE,"AT+CPIN=%s",pin);

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        command,
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for SIM Toolkit event notification handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_sim_AddSimToolkitEventHandler
(
    pa_sim_SimToolkitEventHdlrFunc_t handler,    ///< [IN] The handler function.
    void*                            contextPtr  ///< [IN] The context to be given to the handler.
)
{
    /**
     * It is safe to return a fake reference as this functionality
     * is not used on all platform, but the function is invoked in
     * le_sim_Init()
     */
    return (le_event_HandlerRef_t) 0x01;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for SIM Toolkit event notification
 * handling.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_RemoveSimToolkitEventHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the last SIM Toolkit status.
 *
 * @return
 *      - LE_OK             On success.
 *      - LE_BAD_PARAMETER  A parameter is invalid.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetLastStkStatus
(
    pa_sim_StkEvent_t*  stkStatus  ///< [OUT] last SIM Toolkit event status
)
{
    if (NULL == stkStatus)
    {
        return LE_BAD_PARAMETER;
    }

    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to open a logical channel on the SIM card.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT for unexpected error
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_OpenLogicalChannel
(
    uint8_t* channelPtr  ///< [OUT] channel number
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to close a logical channel on the SIM card.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT for unexpected error
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_CloseLogicalChannel
(
    uint8_t channel  ///< [IN] channel number
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to send an APDU message to the SIM card.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW the response length exceed the maximum buffer length.
 *      - LE_FAULT for unexpected error
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_SendApdu
(
    uint8_t        channel, ///< [IN] Logical channel.
    const uint8_t* apduPtr, ///< [IN] APDU message buffer
    uint32_t       apduLen, ///< [IN] APDU message length in bytes
    uint8_t*       respPtr, ///< [OUT] APDU message response.
    size_t*        lenPtr   ///< [IN,OUT] APDU message response length in bytes.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to send a generic command to the SIM.
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_FAULT          The function failed.
 *      - LE_BAD_PARAMETER  A parameter is invalid.
 *      - LE_NOT_FOUND      - The function failed to select the SIM card for this operation
 *                          - The requested SIM file is not found
 *      - LE_OVERFLOW       Response buffer is too small to copy the SIM answer.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_SendCommand
(
    le_sim_Command_t command,               ///< [IN] The SIM command
    const char*      fileIdentifierPtr,     ///< [IN] File identifier
    uint8_t          p1,                    ///< [IN] Parameter P1 passed to the SIM
    uint8_t          p2,                    ///< [IN] Parameter P2 passed to the SIM
    uint8_t          p3,                    ///< [IN] Parameter P3 passed to the SIM
    const uint8_t*   dataPtr,               ///< [IN] Data command
    size_t           dataNumElements,       ///< [IN] Size of data command
    const char*      pathPtr,               ///< [IN] Path of the elementary file
    uint8_t*         sw1Ptr,                ///< [OUT] SW1 received from the SIM
    uint8_t*         sw2Ptr,                ///< [OUT] SW2 received from the SIM
    uint8_t*         responsePtr,           ///< [OUT] SIM response
    size_t*          responseNumElementsPtr ///< [IN/OUT] Size of response
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to reset the SIM.
 *
 * @return
 *      - LE_OK          On success.
 *      - LE_FAULT       On failure.
 *      - LE_UNSUPPORTED The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_Reset
(
    void
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to write the FPLMN list into the modem.
 *
 * @return
 *      - LE_OK             On success.
 *      - LE_FAULT          On failure.
 *      - LE_BAD_PARAMETER  A parameter is invalid.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_WriteFPLMNList
(
    le_dls_List_t *FPLMNListPtr ///< [IN] List of FPLMN operators
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the number of FPLMN operators present in the list.
 *
 * @return
 *      - LE_OK             On success.
 *      - LE_FAULT          On failure.
 *      - LE_BAD_PARAMETER  A parameter is invalid.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_CountFPLMNOperators
(
    uint32_t*  nbItemPtr     ///< [OUT] number of FPLMN operator found if success.
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to read the FPLMN list.
 *
 * @return
 *      - LE_OK             On success.
 *      - LE_NOT_FOUND      If no FPLMN network is available.
 *      - LE_BAD_PARAMETER  A parameter is invalid.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_ReadFPLMNOperators
(
    pa_sim_FPLMNOperator_t* FPLMNOperatorPtr,   ///< [OUT] FPLMN operators.
    uint32_t* FPLMNOperatorCountPtr             ///< [IN/OUT] FPLMN operator count.
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the last SIM Toolkit status.
 *
 * @return
 *      - LE_OK             On success.
 *      - LE_BAD_PARAMETER  A parameter is invalid.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetLastStkStatus
(
    pa_sim_StkEvent_t*  stkStatus  ///< [OUT] last SIM Toolkit event status
)
{
    if (NULL == stkStatus)
    {
        return LE_BAD_PARAMETER;
    }

    return LE_UNSUPPORTED;
}
