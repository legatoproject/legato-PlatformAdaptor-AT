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
le_result_t __attribute__((weak)) pa_sim_Init
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
uint32_t __attribute__((weak)) pa_sim_CountSlots
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
le_result_t __attribute__((weak)) pa_sim_SelectCard
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
le_result_t __attribute__((weak)) pa_sim_GetSelectedCard
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
le_result_t __attribute__((weak)) pa_sim_GetCardIdentification
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
le_result_t __attribute__((weak)) pa_sim_GetIMSI
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
le_result_t __attribute__((weak)) pa_sim_GetState
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
 * This function retrieves the identifier for the embedded Universal Integrated Circuit Card (EID)
 * (16 digits)
 *
 * @return LE_OK            The function succeeded.
 * @return LE_FAULT         The function failed.
 * @return LE_UNSUPPORTED   The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetCardEID
(
   pa_sim_Eid_t eid               ///< [OUT] the EID value
)
{
    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
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
le_event_HandlerRef_t __attribute__((weak)) pa_sim_AddNewStateHandler
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
le_result_t __attribute__((weak)) pa_sim_RemoveNewStateHandler
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
le_result_t __attribute__((weak)) pa_sim_EnterPIN
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
 * This function set the new PIN code.
 *
 *  - use to set pin code by providing the puk
 *
 * All depends on SIM state which must be retreived by @ref pa_sim_GetState
 *
 * @return LE_FAULT         The function failed to set the value.
 * @return LE_TIMEOUT       No response was received from the SIM card.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_EnterPUK
(
    pa_sim_PukType_t   type, ///< [IN] puk type
    const pa_sim_Puk_t puk,  ///< [IN] PUK code
    const pa_sim_Pin_t pin   ///< [IN] new PIN code
)
{
    char                 command[LE_ATDEFS_COMMAND_MAX_BYTES];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;

    snprintf(command,LE_ATDEFS_COMMAND_MAX_BYTES,"AT+CPIN=%s,%s",puk,pin);

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
 * This function get the remaining attempts of a code.
 *
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t pa_sim_GetRemainingAttempts
(
    uint32_t  idx,          ///< [IN] idx to read
    uint32_t* attemptsPtr  ///< [OUT] The number of attempts still possible
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function get the remaining attempts of a PIN code.
 *
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetPINRemainingAttempts
(
    pa_sim_PinType_t type,         ///< [IN] The PIN type
    uint32_t*        attemptsPtr   ///< [OUT] The number of attempts still possible
)
{
    if (PA_SIM_PIN == type)
    {
        return pa_sim_GetRemainingAttempts(0,attemptsPtr);
    }
    else if (PA_SIM_PIN2 == type)
    {
        return pa_sim_GetRemainingAttempts(1,attemptsPtr);
    }
    else
    {
        return LE_BAD_PARAMETER;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function get the remaining attempts of a PUK code.
 *
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetPUKRemainingAttempts
(
    pa_sim_PukType_t type,         ///< [IN] The PUK type
    uint32_t*        attemptsPtr   ///< [OUT] The number of attempts still possible
)
{
    if (PA_SIM_PUK == type)
    {
        return pa_sim_GetRemainingAttempts(2,attemptsPtr);
    }
    else if (PA_SIM_PUK2 == type)
    {
        return pa_sim_GetRemainingAttempts(3,attemptsPtr);
    }
    else
    {
        return LE_BAD_PARAMETER;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function change a code.
 *
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed to set the value.
 * @return LE_TIMEOUT       No response was received from the SIM card.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_ChangePIN
(
    pa_sim_PinType_t   type,    ///< [IN] The code type
    const pa_sim_Pin_t oldcode, ///< [IN] Old code
    const pa_sim_Pin_t newcode  ///< [IN] New code
)
{
    char                 command[PA_AT_LOCAL_SHORT_SIZE];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;

    if (type==PA_SIM_PIN)
    {
        snprintf(command, PA_AT_LOCAL_SHORT_SIZE, "AT+CPWD=\"SC\",%s,%s", oldcode, newcode);
    }
    else if (type==PA_SIM_PIN2)
    {
        snprintf(command, PA_AT_LOCAL_SHORT_SIZE, "AT+CPWD=\"P2\",%s,%s", oldcode, newcode);
    }
    else
    {
        return LE_BAD_PARAMETER;
    }

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
 * This function enables PIN locking (PIN or PIN2).
 *
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed to set the value.
 * @return LE_TIMEOUT       No response was received from the SIM card.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_EnablePIN
(
    pa_sim_PinType_t   type,  ///< [IN] The pin type
    const pa_sim_Pin_t code   ///< [IN] code
)
{
    char                 command[PA_AT_LOCAL_SHORT_SIZE];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;

    if (type==PA_SIM_PIN)
    {
        snprintf(command, PA_AT_LOCAL_SHORT_SIZE, "at+clck=\"SC\",1,%s", code);
    }
    else if (type==PA_SIM_PIN2)
    {
        snprintf(command, PA_AT_LOCAL_SHORT_SIZE, "at+clck=\"P2\",1,%s", code);
    }
    else
    {
        return LE_BAD_PARAMETER;
    }

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
 * This function disables PIN locking (PIN or PIN2).
 *
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed to set the value.
 * @return LE_TIMEOUT       No response was received from the SIM card.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_DisablePIN
(
    pa_sim_PinType_t   type,  ///< [IN] The code type.
    const pa_sim_Pin_t code   ///< [IN] code
)
{
    char                 command[LE_ATDEFS_COMMAND_MAX_BYTES];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;

    if      (type==PA_SIM_PIN)
    {
        snprintf(command,LE_ATDEFS_COMMAND_MAX_BYTES,"at+clck=\"SC\",0,%s",code);
    }
    else if (type==PA_SIM_PIN2)
    {
        snprintf(command,LE_ATDEFS_COMMAND_MAX_BYTES,"at+clck=\"P2\",0,%s",code);
    }
    else
    {
        return LE_BAD_PARAMETER;
    }

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
 * Get the SIM Phone Number.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the Phone Number can't fit in phoneNumberStr
 *      - LE_FAULT on any other failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetSubscriberPhoneNumber
(
    char*        phoneNumberStr,    ///< [OUT] The phone Number
    size_t       phoneNumberStrSize ///< [IN]  Size of phoneNumberStr
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_OK;
    char                *tokenPtr = NULL;
    char                 responseStr[PA_AT_LOCAL_SHORT_SIZE] = {0};
    int32_t              cmeeMode = pa_utils_GetCmeeMode();
    uint8_t              paramNum;

    if (!phoneNumberStr || !phoneNumberStrSize)
    {
        LE_ERROR("Bad Parameters");
        return LE_FAULT;
    }

    pa_utils_SetCmeeMode(1);
    res = le_atClient_SetCommandAndSend(&cmdRef,
        pa_utils_GetAtDeviceRef(),
        "AT+CNUM",
        "+CNUM:",
        DEFAULT_AT_RESPONSE,
        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        pa_utils_SetCmeeMode(cmeeMode);
        return LE_FAULT;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_SHORT_SIZE);

    if (res != LE_OK || (strcmp(responseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        pa_utils_SetCmeeMode(cmeeMode);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_SHORT_SIZE);

    le_atClient_Delete(cmdRef);
    pa_utils_SetCmeeMode(cmeeMode);
    phoneNumberStr[0] = NULL_CHAR;

    if (res != LE_OK)
    {
        LE_WARN("No MSIDN Provided");
        return LE_OK;
    }

    paramNum = pa_utils_CountAndIsolateLineParameters(responseStr);
    if(paramNum != 4)
    {
        LE_WARN("No MSIDN Provided");
        return LE_OK;
    }
    // Extract the phone number string
    tokenPtr = pa_utils_IsolateLineParameter(responseStr, 3);
    if(!tokenPtr)
    {
        LE_WARN("No MSIDN Provided");
        return LE_OK;
    }

    // Remove the quotation marks
    pa_utils_RemoveQuotationString(tokenPtr);

    res = le_utf8_Copy(phoneNumberStr, tokenPtr, phoneNumberStrSize, NULL);

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Home Network Name information.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the Home Network Name can't fit in nameStr
 *      - LE_FAULT on any other failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetHomeNetworkOperator
(
    char*       nameStr,               ///< [OUT] the home network Name
    size_t      nameStrSize            ///< [IN] the nameStr size
)
{
    le_result_t          res      = LE_OK;
    le_atClient_CmdRef_t cmdRef   = NULL;
    char*                tokenPtr = NULL;
    char*                savePtr  = NULL;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE];

    if (!nameStr)
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+COPS?",
                                        "+COPS:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return LE_FAULT;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_STRING_SIZE);

    if ((res != LE_OK) || (strcmp(responseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the response");
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
        return LE_FAULT;
    }

    // Cut the string for keep just the Home PLMN
    res = LE_FAULT;
    tokenPtr = strtok_r(responseStr, "\"", &savePtr);
    if(tokenPtr)
    {
        tokenPtr = strtok_r(NULL, "\"", &savePtr);
        if(tokenPtr)
        {
            res = le_utf8_Copy(nameStr, tokenPtr, nameStrSize, NULL);
        }
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Home Network MCC MNC.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the Home Network MCC/MNC can't fit in mccPtr and mncPtr
 *      - LE_FAULT for unexpected error
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_GetHomeNetworkMccMnc
(
    char*     mccPtr,                ///< [OUT] Mobile Country Code
    size_t    mccPtrSize,            ///< [IN] mccPtr buffer size
    char*     mncPtr,                ///< [OUT] Mobile Network Code
    size_t    mncPtrSize             ///< [IN] mncPtr buffer size
)
{
    le_result_t res;
    pa_sim_Imsi_t imsi = {0};
    char mccStr[4] = {0};
    char mncStr[4] = {0};

    if ((!mccPtr) || (!mncPtr))
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    res = pa_sim_GetIMSI(imsi);
    if(LE_OK == res)
    {
        strncpy(mccStr, imsi, 3);
        if(0 == strcmp(mccStr, "310"))
        {
            strncpy(mncStr, imsi+3, 3);
        }
        else
        {
            strncpy(mncStr, imsi+3, 2);
        }

        res = le_utf8_Copy(mccPtr, mccStr,  mccPtrSize, NULL);
        if (LE_OK == res)
        {
            res = le_utf8_Copy(mncPtr, mncStr, mncPtrSize, NULL);
        }
    }

    return res;
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
le_result_t __attribute__((weak)) pa_sim_OpenLogicalChannel
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
le_result_t __attribute__((weak)) pa_sim_CloseLogicalChannel
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
le_result_t __attribute__((weak)) pa_sim_SendApdu
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
 * This function must be called to trigger a SIM refresh.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT for unexpected error
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_Refresh
(
    void
)
{
    return LE_FAULT;
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
le_event_HandlerRef_t __attribute__((weak)) pa_sim_AddSimToolkitEventHandler
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
le_result_t __attribute__((weak)) pa_sim_RemoveSimToolkitEventHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to confirm a SIM Toolkit command.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t __attribute__((weak)) pa_sim_ConfirmSimToolkitCommand
(
    bool  confirmation  ///< [IN] true to accept, false to reject
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
le_result_t __attribute__((weak)) pa_sim_SendCommand
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
le_result_t pa_sim_WriteFPLMNList
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
le_result_t pa_sim_CountFPLMNOperators
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
le_result_t pa_sim_ReadFPLMNOperators
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
 * Enable or disable the automatic SIM selection
 *
 * @return
 *      - LE_OK          Function succeeded.
 *      - LE_FAULT       Function failed to execute.
 *      - LE_UNSUPPORTED The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_SetAutomaticSelection
(
    bool    enable    ///< [IN] True if the feature needs to be enabled. False otherwise.
)
{
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the automatic SIM selection
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_FAULT          Function failed to execute.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 *      - LE_UNSUPPORTED    The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetAutomaticSelection
(
    bool*    enablePtr    ///< [OUT] True if the feature is enabled. False otherwise.
)
{
    return LE_UNSUPPORTED;
}
