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
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
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

    if ((res != LE_OK) || (strcmp(responseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
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
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
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

    if ((res != LE_OK) || (strcmp(responseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
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
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
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
