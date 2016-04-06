/** @file pa_mcc.c
 *
 * AT implementation of c_pa_mcc API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include "legato.h"

#include "pa_mcc.h"
#include "pa_common_local.h"

#include "at/inc/le_atClient.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions.
//--------------------------------------------------------------------------------------------------

#define DEFAULT_CALLEVENTDATA_POOL_SIZE 1

static le_event_Id_t          InternalEventCall;
static le_event_Id_t          EventCallData;

static le_event_HandlerRef_t  CallDataHandlerRef=NULL;

static le_atClient_CmdRef_t  ATReqCallRef = NULL;

//--------------------------------------------------------------------------------------------------
// APIs.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister all unsocilited.
 *
 *
 */
//--------------------------------------------------------------------------------------------------
static void UnregisterDial
(
    void
)
{
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"OK");
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"NO CARRIER");
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"BUSY");
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"NO ANSWER");

    if (ATReqCallRef) {
        le_mem_Release(ATReqCallRef);   // Release e_le_atClient_mgr_Object_CreateATCommand
        ATReqCallRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to translate the code status for +WIND parsing
 *
 * @return true : eventPtr and terminationPtr are filled
 * @return false
 */
//--------------------------------------------------------------------------------------------------
static bool ChecktStatus_WindCode
(
    const char      *valuePtr,   ///< [IN] the value to change
    le_mcc_Event_t *eventPtr, ///< [OUT] the event for WIND
    le_mcc_TerminationReason_t *terminationPtr  ///< [OUT] Termination reason
)
{
    bool result = true;
    uint32_t value = atoi(valuePtr);

    switch (value)
    {
        case 2: /* the calling party is alerting */
        {
            *terminationPtr = LE_MCC_TERM_UNDEFINED;
            *eventPtr = LE_MCC_EVENT_ALERTING;
            break;
        }
        case 5: /* a call <idx> has been created (after ATD or +CCWA...) */
        {
            *eventPtr = LE_MCC_EVENT_ALERTING;
            break;
        }
        case 6: /* a call <idx> has been released, after a NO CARRIER,
                   a "+CSSU: 5" indication, or after the release of a call waiting */
        {
            *eventPtr = LE_MCC_EVENT_TERMINATED;
            break;
        }
        default:
        {
            result = false;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to translate the code status for +CSSU parsing
 *
 * @return true : eventPtr and terminationPtr are filled
 * @return false
 */
//--------------------------------------------------------------------------------------------------
static bool ChecktStatus_CssuCode
(
    const char      *valuePtr,   ///< [IN] the value to change
    le_mcc_Event_t *eventPtr, ///< [OUT] the event for CSSU
    le_mcc_TerminationReason_t *terminationPtr  ///< [OUT] Termination reason
)
{
    bool result = true;
    uint32_t value = atoi(valuePtr);

    *terminationPtr = LE_MCC_TERM_UNDEFINED;
    switch (value)
    {
        case 5: /* call on hold has been released (during a voice call) */
        {
            *eventPtr = LE_MCC_EVENT_TERMINATED;
            *terminationPtr = LE_MCC_TERM_REMOTE_ENDED;
            break;
        }
        case 7:
        {
            *eventPtr = LE_MCC_EVENT_ALERTING;
            break;
        }
        default:
        {
            result = false;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for an call.
 *
 *
 */
//--------------------------------------------------------------------------------------------------
static void MCCInternalHandler(void* reportPtr) {
    char* unsolPtr = reportPtr;

    pa_mcc_CallEventData_t callData;
    memset(&callData,0,sizeof(callData));

    le_atClient_cmd_CountLineParameter(unsolPtr);

    callData.terminationEvent = LE_MCC_TERM_UNDEFINED;

    if ( (FIND_STRING("OK",unsolPtr)) )
    {
        callData.event = LE_MCC_EVENT_CONNECTED;
        le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"OK");
        if (ATReqCallRef)
        {
            le_mem_Release(ATReqCallRef);   // Release e_le_atClient_mgr_Object_CreateATCommand
            ATReqCallRef = NULL;
        }
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }
    else if ( (FIND_STRING("NO CARRIER",unsolPtr)) )
    {
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_REMOTE_ENDED;
        UnregisterDial();
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }
    else if ( (FIND_STRING("BUSY",unsolPtr)) )
    {
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_USER_BUSY;
        UnregisterDial();
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }
    else if ( (FIND_STRING("NO ANSWER",unsolPtr)) )
    {
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_REMOTE_ENDED;
        UnregisterDial();
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }
    else if ( (FIND_STRING("RING",unsolPtr)) )
    {
        callData.event = LE_MCC_EVENT_INCOMING;
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }
    else if ( (FIND_STRING("+CRING:",le_atClient_cmd_GetLineParameter(unsolPtr,1))) )
    {
        callData.event = LE_MCC_EVENT_INCOMING;

        le_event_Report(EventCallData,&callData,sizeof(callData));
    }
    else if ( (FIND_STRING("+WIND:",le_atClient_cmd_GetLineParameter(unsolPtr,1))) )
    {
    if ( ChecktStatus_WindCode(le_atClient_cmd_GetLineParameter(unsolPtr,2),
            &(callData.event),&(callData.terminationEvent)))
        {
            le_event_Report(EventCallData,&callData,sizeof(callData));
        }
    }
    else if ( (FIND_STRING("+CSSU:",le_atClient_cmd_GetLineParameter(unsolPtr,1))) )
    {
    if ( ChecktStatus_CssuCode(le_atClient_cmd_GetLineParameter(unsolPtr,2),
            &(callData.event),&(callData.terminationEvent)))
        {
            le_event_Report(EventCallData,&callData,sizeof(callData));
        }
    }
    else
    {
        LE_WARN("this pattern is not expected -%s-",unsolPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the mcc module
 *
 * @return LE_FAULT         The function failed to initialize the module.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_Init
(
    void
)
{
//     if (AllPorts[ATPORT_COMMAND]==NULL) {
//         LE_WARN("Modem Call Control module is not initialize in this session");
//         return LE_FAULT;
//     }

    EventCallData = le_event_CreateId("MCCEventCallData",LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES);

    InternalEventCall = le_event_CreateId("MCCInternalEventCall",sizeof(char));
    le_event_AddHandler("MCCUnsolHandler",InternalEventCall  ,MCCInternalHandler);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set CSSU unsolicited code
 *
 * @return LE_FAULT  The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetCssuIndicator
(
    void
)
{
    const char* commandPtr     = "AT+CSSN=0,1";
    const char* interRespPtr   = "\0";
    const char* respPtr        = "\0";

    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t res;

    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"+CSSU:",false);

    res = le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set sierrawireless indication WIND (2,5,6)
 *
 * @return LE_FAULT         The function failed to set WIND indicator.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetIndicator()
{
    uint32_t wind;

    if (SetCssuIndicator() != LE_OK )
    {
        return LE_FAULT;
    }

    if (pa_common_GetWindIndicator(&wind) != LE_OK)
    {
        return LE_FAULT;
    }

    if (pa_common_SetWindIndicator(wind|2) != LE_OK)
    {
        return LE_FAULT;
    }

    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"+WIND: 2",false);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Call event notifications.
 *
 * @return LE_FAULT         The function failed to register the handler.
 * @return LE_DUPLICATE     There is already a handler registered.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_SetCallEventHandler
(
    pa_mcc_CallEventHandlerFunc_t   handlerFuncPtr
)
{
    LE_DEBUG("Set new Call Control handler");

    LE_FATAL_IF(handlerFuncPtr==NULL,"new Call Control handler is NULL");

    if (CallDataHandlerRef)
    {
        LE_WARN("CallEvent Already set");
        return LE_DUPLICATE;
    }

    if (SetIndicator() != LE_OK)
    {
        LE_WARN("Cannot set SierraWireless indication");
        return LE_FAULT;
    }

    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"RING",false);
    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"+CRING:",false);

    CallDataHandlerRef = le_event_AddHandler("NewCallControlHandler",EventCallData,(le_event_HandlerFunc_t) handlerFuncPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for incoming calls handling.
 *
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mcc_ClearCallEventHandler
(
    void
)
{
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"RING");
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"+CRING:");
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"+WIND: 2");
    le_atClient_RemoveUnsolicitedResponseHandler(InternalEventCall,"+CSSU:");

    le_event_RemoveHandler(CallDataHandlerRef);
    CallDataHandlerRef = NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set a voice call.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_BUSY          A call is already ongoing.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_VoiceDial
(
    const char*    pn,                      ///< [IN] The phone number.
    pa_mcc_clir_t  clir,                    ///< [IN] The CLIR supplementary service subscription.
    pa_mcc_cug_t   cug,                     ///< [IN] The CUG supplementary service information.
    uint8_t*       callIdPtr,               ///< [OUT] The outgoing call ID.
    le_mcc_TerminationReason_t*  errorPtr   ///< [OUT] Call termination error.
)
{
    char command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char* interRespPtr   = "\0";
    const char* respPtr        = "\0";

    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t res;

    if (ATReqCallRef)
    {
        LE_WARN("There is already an Voice dial in progress");
        return LE_BUSY;
    }

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"ATD%s%c%c;",pn,(clir==PA_MCC_ACTIVATE_CLIR)?'i':'I',(cug==PA_MCC_ACTIVATE_CUG)?'g':'G');

    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"OK",false);
    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"NO CARRIER",false);
    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"BUSY",false);
    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"NO ANSWER",false);

    res = le_atClient_SetCommandAndSend(&cmdRef,(char*)command,interRespPtr,respPtr,DEFAULT_TIMEOUT);

    // TODO: populate callIdPtr
    *callIdPtr = 0;

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to answer a call.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_Answer
(
    uint8_t  callId     ///< [IN] The call ID to answer
)
{
    const char* commandPtr     = "ATA";
    const char* interRespPtr   = "\0";
    const char* respPtr        = "\0";

    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t res;

    if (ATReqCallRef)
    {
        le_mem_Release(ATReqCallRef);   // Release e_le_atClient_mgr_Object_CreateATCommand
        ATReqCallRef = NULL;
    }

    le_atClient_AddUnsolicitedResponseHandler(InternalEventCall,"NO CARRIER",false);

    res = le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_TIMEOUT);

    if (res == LE_OK)
    {
        pa_mcc_CallEventData_t callData;
        memset(&callData,0,sizeof(callData));
        callData.event = LE_MCC_EVENT_CONNECTED;
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to disconnect the remote user.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_HangUp
(
    uint8_t  callId     ///< [IN] The call ID to hangup
)
{
    const char* commandPtr     = "ATH0";
    const char* interRespPtr   = "\0";
    const char* respPtr        = "\0";

    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t res;

    UnregisterDial();

    res = le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_TIMEOUT);

    if (res == LE_OK)
    {
        pa_mcc_CallEventData_t callData;
        memset(&callData,0,sizeof(callData));
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_LOCAL_ENDED;
        le_event_Report(EventCallData,&callData,sizeof(callData));
    }

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to end all the ongoing calls.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_HangUpAll
(
    void
)
{
    return LE_FAULT;
}
