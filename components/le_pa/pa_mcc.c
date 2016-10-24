/** @file pa_mcc.c
 *
 * AT implementation of c_pa_mcc API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_mcc.h"
#include "pa_utils_local.h"
#include "pa_at_local.h"


//--------------------------------------------------------------------------------------------------
/**
 * Call event
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t          CallEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Call handler reference
 */
//--------------------------------------------------------------------------------------------------
static le_event_HandlerRef_t  CallHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * AT call request reference
 */
//--------------------------------------------------------------------------------------------------
static le_atClient_CmdRef_t   AtCmdReqRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Unsolicited references
 */
//--------------------------------------------------------------------------------------------------
le_atClient_UnsolicitedResponseHandlerRef_t UnsolOkRef = NULL;
le_atClient_UnsolicitedResponseHandlerRef_t UnsolNoCarrierRef = NULL;
le_atClient_UnsolicitedResponseHandlerRef_t UnsolBusyRef = NULL;
le_atClient_UnsolicitedResponseHandlerRef_t UnsolNoAnswerRef = NULL;
le_atClient_UnsolicitedResponseHandlerRef_t UnsolRingRef = NULL;
le_atClient_UnsolicitedResponseHandlerRef_t UnsolCringRef = NULL;

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
    if (UnsolOkRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolOkRef);
        UnsolOkRef = NULL;
    }

    if (UnsolNoCarrierRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolNoCarrierRef);
        UnsolNoCarrierRef = NULL;
    }

    if (UnsolBusyRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolBusyRef);
        UnsolBusyRef = NULL;
    }

    if (UnsolNoAnswerRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolNoAnswerRef);
        UnsolNoAnswerRef = NULL;
    }

    if (AtCmdReqRef)
    {
        le_mem_Release(AtCmdReqRef);
        AtCmdReqRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to translate the code status for +CSSU parsing
 *
 * @return true : eventPtr and terminationPtr are filled
 * @return false
 */
//--------------------------------------------------------------------------------------------------
static bool CheckCssuCode
(
    const char*                 valuePtr,       ///< [IN] the value to change
    le_mcc_Event_t*             eventPtr,       ///< [OUT] the event for CSSU
    le_mcc_TerminationReason_t* terminationPtr  ///< [OUT] Termination reason
)
{
    bool result = true;

   *terminationPtr = LE_MCC_TERM_UNDEFINED;
    switch (atoi(valuePtr))
    {
        case 5: /* call on hold has been released (during a voice call) */
        {
            *eventPtr       = LE_MCC_EVENT_TERMINATED;
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
 * This function is a handler for call events.
 *
 */
//--------------------------------------------------------------------------------------------------
static void PaMccUnsolHandler
(
    const char* unsolPtr,
    void* contextPtr
)
{
    pa_mcc_CallEventData_t callData;

    memset(&callData,0,sizeof(callData));
    LE_DEBUG("Handler received -%s-",unsolPtr);

    pa_utils_CountAndIsolateLineParameters( (char*) unsolPtr);

    callData.terminationEvent = LE_MCC_TERM_UNDEFINED;

    if ((FIND_STRING("OK",unsolPtr)))
    {
        callData.event = LE_MCC_EVENT_CONNECTED;

        if (UnsolOkRef)
        {
            le_atClient_RemoveUnsolicitedResponseHandler(UnsolOkRef);
            UnsolOkRef = NULL;
        }

        if (AtCmdReqRef)
        {
            le_mem_Release(AtCmdReqRef);
            AtCmdReqRef = NULL;
        }
        le_event_Report(CallEventId,&callData,sizeof(callData));
    }
    else if ((FIND_STRING("NO CARRIER",unsolPtr)))
    {
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_REMOTE_ENDED;
        UnregisterDial();
        le_event_Report(CallEventId,&callData,sizeof(callData));
    }
    else if ((FIND_STRING("BUSY",unsolPtr)) )
    {
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_USER_BUSY;
        UnregisterDial();
        le_event_Report(CallEventId,&callData,sizeof(callData));
    }
    else if ((FIND_STRING("NO ANSWER",unsolPtr)))
    {
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_REMOTE_ENDED;
        UnregisterDial();
        le_event_Report(CallEventId,&callData,sizeof(callData));
    }
    else if ((FIND_STRING("RING",unsolPtr)))
    {
        callData.event = LE_MCC_EVENT_INCOMING;
        le_event_Report(CallEventId,&callData,sizeof(callData));
    }
    else if ((FIND_STRING("+CRING:",pa_utils_IsolateLineParameter(unsolPtr,1))))
    {
        callData.event = LE_MCC_EVENT_INCOMING;
        le_event_Report(CallEventId,&callData,sizeof(callData));
    }
    else if ((FIND_STRING("+CSSU:",pa_utils_IsolateLineParameter(unsolPtr,1))))
    {
    if (CheckCssuCode(pa_utils_IsolateLineParameter(unsolPtr,2),
        &(callData.event), &(callData.terminationEvent)))
        {
            le_event_Report(CallEventId,&callData,sizeof(callData));
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
    CallEventId = le_event_CreateId("CallEventId",sizeof(pa_mcc_CallEventData_t));

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
    pa_mcc_CallEventHandlerFunc_t handlerFuncPtr
)
{
    LE_DEBUG("Set new Call Control handler");

    LE_FATAL_IF(handlerFuncPtr==NULL,"new Call Control handler is NULL");

    if (CallHandlerRef)
    {
        LE_WARN("CallEvent Already set");
        return LE_DUPLICATE;
    }

    UnsolRingRef = le_atClient_AddUnsolicitedResponseHandler(   "RING",
                                                                pa_at_GetAtDeviceRef(),
                                                                PaMccUnsolHandler,
                                                                NULL,
                                                                1   );

    UnsolCringRef = le_atClient_AddUnsolicitedResponseHandler(  "+CRING:",
                                                                pa_at_GetAtDeviceRef(),
                                                                PaMccUnsolHandler,
                                                                NULL,
                                                                1   );

    CallHandlerRef = le_event_AddHandler("NewCallControlHandler",
                                             CallEventId,
                                             (le_event_HandlerFunc_t) handlerFuncPtr);
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
    if (UnsolRingRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolRingRef);
        UnsolRingRef = NULL;
    }

    if (UnsolCringRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolCringRef);
        UnsolCringRef = NULL;
    }

    //~le_atClient_RemoveUnsolicitedResponseHandler(UnsolCssuRef);

    le_event_RemoveHandler(CallHandlerRef);
    CallHandlerRef = NULL;
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
    const char*                 phoneNumberPtr, ///< [IN] The phone number.
    pa_mcc_clir_t               clir,           ///< [IN] The CLIR supplementary service subscription.
    pa_mcc_cug_t                cug,            ///< [IN] The CUG supplementary service information.
    uint8_t*                    callIdPtr,      ///< [OUT] The outgoing call ID.
    le_mcc_TerminationReason_t* errorPtr        ///< [OUT] Call termination error.
)
{
    char                 command[LE_ATDEFS_COMMAND_MAX_BYTES];
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

    if (AtCmdReqRef)
    {
        LE_WARN("There is already an Voice dial in progress");
        return LE_BUSY;
    }

    snprintf(command,
             LE_ATDEFS_COMMAND_MAX_BYTES,
             "ATD%s%c%c;",
             phoneNumberPtr,
             (clir==PA_MCC_DEACTIVATE_CLIR)?'i':'I',
             (cug==PA_MCC_ACTIVATE_CUG)?'g':'G');

    UnsolOkRef = le_atClient_AddUnsolicitedResponseHandler( "OK",
                                                            pa_at_GetAtDeviceRef(),
                                                            PaMccUnsolHandler,
                                                            NULL,
                                                            1 );

    UnsolNoCarrierRef = le_atClient_AddUnsolicitedResponseHandler(  "NO CARRIER",
                                                                    pa_at_GetAtDeviceRef(),
                                                                    PaMccUnsolHandler,
                                                                    NULL,
                                                                    1   );


    UnsolBusyRef = le_atClient_AddUnsolicitedResponseHandler(   "BUSY",
                                                                pa_at_GetAtDeviceRef(),
                                                                PaMccUnsolHandler,
                                                                NULL,
                                                                1   );

    UnsolNoAnswerRef = le_atClient_AddUnsolicitedResponseHandler(   "NO ANSWER",
                                                                    pa_at_GetAtDeviceRef(),
                                                                    PaMccUnsolHandler,
                                                                    NULL,
                                                                    1   );


    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_at_GetAtDeviceRef(),
                                        (char*)command,
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    *callIdPtr = 0;
    if (res == LE_OK)
    {
        le_atClient_Delete(cmdRef);
    }
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
    uint8_t callId     ///< [IN] The call ID to answer
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

    if (AtCmdReqRef)
    {
        le_mem_Release(AtCmdReqRef);   // Release e_le_atClient_mgr_Object_CreateATCommand
        AtCmdReqRef = NULL;
    }

    UnsolNoCarrierRef = le_atClient_AddUnsolicitedResponseHandler( "NO CARRIER",
                                                                   pa_at_GetAtDeviceRef(),
                                                                   PaMccUnsolHandler,
                                                                   NULL,
                                                                   1 );

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_at_GetAtDeviceRef(),
                                        "ATA",
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res == LE_OK)
    {
        pa_mcc_CallEventData_t callData;
        memset(&callData,0,sizeof(callData));
        callData.event = LE_MCC_EVENT_CONNECTED;
        le_event_Report(CallEventId,&callData,sizeof(callData));
        le_atClient_Delete(cmdRef);
    }
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
    uint8_t callId     ///< [IN] The call ID to hangup
)
{
    return pa_mcc_HangUpAll();
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
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

    UnregisterDial();

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_at_GetAtDeviceRef(),
                                        "ATH0",
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res == LE_OK)
    {
        pa_mcc_CallEventData_t callData;
        memset(&callData,0,sizeof(callData));
        callData.event = LE_MCC_EVENT_TERMINATED;
        callData.terminationEvent = LE_MCC_TERM_LOCAL_ENDED;
        le_event_Report(CallEventId,&callData,sizeof(callData));
        le_atClient_Delete(cmdRef);
    }
    return res;
}
