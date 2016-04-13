/** @file pa_sms.c
 *
 * AT implementation of c_pa_sms API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "legato.h"

#include "pa_sms.h"
#include "pa_sms_local.h"
#include "pa_utils_local.h"

#include "le_atClient.h"


#define DEFAULT_SMSREF_POOL_SIZE    1

static le_mem_PoolRef_t       SmsRefPoolRef;
static le_event_Id_t          EventUnsolicitedId;
static le_event_Id_t          EventNewSMSId;
static le_event_HandlerRef_t  NewSMSHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to check the CMTI unsolicited.
 *  parsing is "+CMTI: mem,index"
 *  parsing is "+CBMI: mem,index"
 *  parsing is "+CDSI: mem,index"
 *
 * @return true and msgRef is filled.
 * @return false.
 */
//--------------------------------------------------------------------------------------------------
static bool Check_smsRefCode
(
    char      *line,     ///< [IN] line to parse
    uint32_t  *msgRef    ///< [OUT] message Reference
)
{
    if (pa_utils_CountAndIsolateLineParameters(line))
    {
        *msgRef = atoi(pa_utils_IsolateLineParameter(line,3));

        LE_DEBUG("SMS message reference %d",*msgRef);
        return true;
    }
    else
    {
        LE_WARN("SMS message reference cannot be decoded %s",line);
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to check SMS reception.
 *
 * @return true and msgRef is filled.
 * @return false.
 */
//--------------------------------------------------------------------------------------------------
static bool CheckSMSUnsolicited
(
    char      *line,       ///< [IN] line to parse
    uint32_t  *msgRef      ///< [OUT] Message reference in memory
)
{
    bool res = false;

    if (FIND_STRING("+CMTI:",line))
    {
        res = Check_smsRefCode(line,msgRef);
    }
    else if (FIND_STRING("+CBMI:",line))
    {
        res = Check_smsRefCode(line,msgRef);
    }
    else if (FIND_STRING("+CDSI:",line))
    {
        res = Check_smsRefCode(line,msgRef);
    }
    else
    {
        LE_DEBUG("this pattern is not expected -%s-",line);
        res = false;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to send event to all registered handler
 *
 */
//--------------------------------------------------------------------------------------------------
static void ReportMsgRef
(
    uint32_t  msgRef  ///< [IN] message reference
)
{
    pa_sms_NewMessageIndication_t messageIndication = {0};

    messageIndication.msgIndex = msgRef;
    messageIndication.protocol = PA_SMS_PROTOCOL_GSM; // @todo Hard-coded

    LE_DEBUG("Send new SMS Event with index %d in memory and protocol %d",
             messageIndication.msgIndex,
             messageIndication.protocol);
    le_event_Report(EventNewSMSId,&messageIndication, sizeof(messageIndication));
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for a new message reception handling.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SMSUnsolHandler
(
    void* reportPtr
)
{
    char* unsolPtr = reportPtr;

    uint32_t msgIdx;

    if (CheckSMSUnsolicited(unsolPtr,&msgIdx))
    {
        ReportMsgRef(msgIdx);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the sms module.
 *
 * @return LE_FAULT         The function failed to initialize the module.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_Init
(
    void
)
{
    EventUnsolicitedId = le_event_CreateId("SMSEventIdUnsol",LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES);
    EventNewSMSId = le_event_CreateId("SMSEventIdNewSMS",sizeof(pa_sms_NewMessageIndication_t));

    le_event_AddHandler("SMSUnsolHandler",EventUnsolicitedId  ,SMSUnsolHandler);

    SmsRefPoolRef = le_mem_CreatePool("smsRefPool",sizeof(uint32_t));
    SmsRefPoolRef = le_mem_ExpandPool(SmsRefPoolRef,DEFAULT_SMSREF_POOL_SIZE);

    NewSMSHandlerRef = NULL;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for a new message reception handling.
 *
 * @return LE_BAD_PARAMETER The parameter is invalid.
 * @return LE_FAULT         The function failed to register a new handler.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_SetNewMsgHandler
(
    pa_sms_NewMsgHdlrFunc_t msgHandler ///< [IN] The handler function to handle a new message reception.
)
{
    LE_DEBUG("Set new SMS message handler");

    if (msgHandler == NULL)
    {
        LE_WARN("new SMS message handler is NULL");
        return LE_BAD_PARAMETER;
    }

    if (NewSMSHandlerRef != NULL)
    {
        LE_WARN("new SMS message handler has already been set");
        return LE_FAULT;
    }

    NewSMSHandlerRef = le_event_AddHandler("NewSMSHandler",
                                           EventNewSMSId,
                                           (le_event_HandlerFunc_t) msgHandler);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for a new message reception handling.
 *
 * @return LE_FAULT         The function failed to unregister the handler.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_ClearNewMsgHandler
(
    void
)
{
    le_event_RemoveHandler(NewSMSHandlerRef);
    NewSMSHandlerRef = NULL;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize pattern matching for unsolicited message indicator.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SetNewMsgIndicLocal
(
    pa_sms_NmiMt_t   mt,     ///< [IN] Result code indication routing for SMS-DELIVER indications.
    pa_sms_NmiBm_t   bm,     ///< [IN] Rules for storing the received CBMs (Cell Broadcast Message) types.
    pa_sms_NmiDs_t   ds      ///< [IN] SMS-STATUS-REPORTs routing.
)
{
    le_atClient_RemoveUnsolicitedResponseHandler(EventUnsolicitedId,"+CMTI:");
    le_atClient_RemoveUnsolicitedResponseHandler(EventUnsolicitedId,"+CMT:");
    le_atClient_RemoveUnsolicitedResponseHandler(EventUnsolicitedId,"+CBMI:");
    le_atClient_RemoveUnsolicitedResponseHandler(EventUnsolicitedId,"+CBM:");
    le_atClient_RemoveUnsolicitedResponseHandler(EventUnsolicitedId,"+CDS:");
    le_atClient_RemoveUnsolicitedResponseHandler(EventUnsolicitedId,"+CDSI:");

    switch (mt)
    {
        case PA_SMS_MT_0:
        {
            break;
        }
        case PA_SMS_MT_1:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CMTI:",false);
            break;
        }
        case PA_SMS_MT_2:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CMT:",true);
            break;
        }
        case PA_SMS_MT_3:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CMTI:",false);
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CMT:",true);
            break;
        }
        default:
        {
            LE_WARN("mt %d does not exist",mt);
        }
    }

    switch (bm)
    {
        case PA_SMS_BM_0:
        {
            break;
        }
        case PA_SMS_BM_1:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CBMI:",false);
            break;
        }
        case PA_SMS_BM_2:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CBM:",true);
            break;
        }
        case PA_SMS_BM_3:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CBMI:",false);
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CBM:",true);
            break;
        }
        default:
        {
            LE_WARN("bm %d does not exist",bm);
        }
    }

    switch (ds)
    {
        case PA_SMS_DS_0:
        {
            break;
        }
        case PA_SMS_DS_1:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CDS:",true);
            break;
        }
        case PA_SMS_DS_2:
        {
            le_atClient_AddUnsolicitedResponseHandler(EventUnsolicitedId,"+CDSI:",false);
            break;
        }
        default:
        {
            LE_WARN("bm %d does not exist",bm);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function selects the procedure for message reception from the network (New Message
 * Indication settings).
 *
 * @return LE_FAULT         The function failed to select the procedure for message reception.
 * @return LE_TIMEOUT       No response was received from the Modem.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_SetNewMsgIndic
(
    pa_sms_NmiMode_t mode,   ///< [IN] Processing of unsolicited result codes.
    pa_sms_NmiMt_t   mt,     ///< [IN] Result code indication routing for SMS-DELIVER indications.
    pa_sms_NmiBm_t   bm,     ///< [IN] Rules for storing the received CBMs (Cell Broadcast Message) types.
    pa_sms_NmiDs_t   ds,     ///< [IN] SMS-STATUS-REPORTs routing.
    pa_sms_NmiBfr_t  bfr     ///< [IN] TA buffer of unsolicited result codes mode.
)
{
    char                 command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;


    SetNewMsgIndicLocal(mt,bm,ds);

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+CNMI=%d,%d,%d,%d,%d", mode, mt,bm,ds,bfr);

    res = le_atClient_SetCommandAndSend(&cmdRef,command,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the New Message Indication settings.
 *
 * @return LE_FAULT         The function failed to get the New Message Indication settings.
 * @return LE_BAD_PARAMETER Bad parameter, one is NULL.
 * @return LE_TIMEOUT       No response was received from the Modem.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_GetNewMsgIndic
(
    pa_sms_NmiMode_t* modePtr,   ///< [OUT] Processing of unsolicited result codes.
    pa_sms_NmiMt_t*   mtPtr,     ///< [OUT] Result code indication routing for SMS-DELIVER indications.
    pa_sms_NmiBm_t*   bmPtr,     ///< [OUT] Rules for storing the received CBMs (Cell Broadcast Message) types.
    pa_sms_NmiDs_t*   dsPtr,     ///< [OUT] SMS-STATUS-REPORTs routing.
    pa_sms_NmiBfr_t*  bfrPtr     ///< [OUT] Terminal Adaptor buffer of unsolicited result codes mode.
)
{
    const char*          commandPtr   = "AT+CNMI?";
    const char*          interRespPtr = "+CNMI:";
    const char*          respPtr      = "\0";
    char*                tokenPtr     = NULL;
    char*                rest         = NULL;
    char*                savePtr      = NULL;
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;
    char                 intermediateResponse[50];
    char                 finalResponse[50];

    if (!modePtr && !mtPtr && !bmPtr && !dsPtr && !bfrPtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    res = le_atClient_GetFinalResponse(cmdRef,finalResponse,50);
    if ((res != LE_OK) || (strcmp(finalResponse, "OK") != 0))
    {
        LE_ERROR("Function failed !");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,intermediateResponse,50);

    rest = intermediateResponse+strlen("+CNMI: ");

    tokenPtr = strtok_r(rest, ",", &savePtr);
    *modePtr = (pa_sms_NmiMode_t)atoi(tokenPtr);

    tokenPtr = strtok_r(NULL, ",", &savePtr);
    *mtPtr   = (pa_sms_NmiMt_t)  atoi(tokenPtr);

    tokenPtr = strtok_r(NULL, ",", &savePtr);
    *bmPtr   = (pa_sms_NmiBm_t)  atoi(tokenPtr);

    tokenPtr = strtok_r(NULL, ",", &savePtr);
    *dsPtr   = (pa_sms_NmiDs_t)  atoi(tokenPtr);

    tokenPtr = strtok_r(NULL, ",", &savePtr);
    *bfrPtr  = (pa_sms_NmiBfr_t) atoi(tokenPtr);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function sets the Preferred Message Format (PDU or Text mode).
 *
 * @return LE_FAULT         The function failed to sets the Preferred Message Format.
 * @return LE_TIMEOUT       No response was received from the Modem.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_SetMsgFormat
(
    le_sms_Format_t   format   ///< [IN] The preferred message format.
)
{
    char                 command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+CMGF=%d",format);

    res = le_atClient_SetCommandAndSend(&cmdRef,command,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function sends a message in PDU mode.
 *
 * @return LE_FAULT           The function failed to send a message in PDU mode.
 * @return LE_BAD_PARAMETER   The parameters are invalid.
 * @return LE_TIMEOUT         No response was received from the Modem.
 * @return a positive value   The function succeeded. The value represents the message reference.
 */
//--------------------------------------------------------------------------------------------------
int32_t pa_sms_SendPduMsg
(
    pa_sms_Protocol_t        protocol,   ///< [IN] protocol to use
    uint32_t                 length,     ///< [IN] The length of the TP data unit in bytes.
    const uint8_t           *dataPtr,    ///< [IN] The message.
    uint32_t                 timeout,    ///< [IN] Timeout in seconds.
    pa_sms_SendingErrCode_t *errorCode   ///< [OUT] The error code.
)
{
//     int32_t result = LE_FAULT;
//     le_atClient_CmdRef_t atReqRef;
//     le_atClient_CmdSyncResultRef_t  resRef = NULL;
//     char atcommand[LE_ATCLIENT_CMD_SIZE_MAX_LEN] ;
//
//     const char* interRespPtr[] = {"+CMGS:",NULL};
//     const char* finalRespOkPtr[] = {"OK",NULL};
//     const char* finalRespKoPtr[] = {"ERROR","+CME ERROR:","+CMS ERROR:","TIMEOUT",NULL};
//
//     char hexString[LE_SMS_PDU_MAX_BYTES*2+1]={0};
//     uint32_t hexStringSize;
//
//     if (!dataPtr)
//     {
//         LE_WARN("One parameter is NULL");
//         return LE_BAD_PARAMETER;
//     }
//
//     snprintf(atcommand,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"at+cmgs=%d",length);
//
//     // TODO: check the smsc field !
//     hexStringSize = le_hex_BinaryToString(dataPtr,length+1,hexString,sizeof(hexString));
//
//     atReqRef = CreateCmd();
//     AddCommand(atReqRef,atcommand,false);
//     AddData(atReqRef,hexString,hexStringSize);
//     SetTimer(atReqRef,30000,GetTimerExpiryHandler());
//     AddIntermediateResp(atReqRef,GetIntermediateEventId(),interRespPtr);
//     AddFinalResp(atReqRef,GetFinalEventId(),finalRespOkPtr);
//     AddFinalResp(atReqRef,GetFinalEventId(),finalRespKoPtr);
//
//     resRef = SendCommand(AllPorts[ATPORT_COMMAND],atReqRef);
//
//     result = le_atClient_CheckCommandResult(resRef,finalRespOkPtr,finalRespKoPtr);
//     if ( result != LE_OK )
//     {
//         le_mem_Release(atReqRef);  // Release le_atClient_cmdsync_SetCommand
//         le_mem_Release(resRef);     // Release SendCommand
//         return result;
//     }
//     // If there is more than one line then it mean that the command is OK so the first line is
//     // the intermediate one
//     // the intermediate one
//     if (le_atClient_GetNumLines(resRef) == 2)
//     {
//         // it parse just the first line because of '\0'
//         char* line = GetLine(resRef,0);
//         uint32_t numParam = pa_utils_CountAndIsolateLineParameters(line);
//         // it parse just the first line because of '\0'
//
//         if (FIND_STRING("+CMGS:",pa_utils_IsolateLineParameter(line,1)))
//         {
//             if (numParam==2)
//             {
//                 result=atoi(pa_utils_IsolateLineParameter(line,2));
//             } else
//             {
//                 LE_WARN("this pattern is not expected");
//                 result = LE_FAULT;
//             }
//         } else {
//             LE_WARN("this pattern is not expected");
//             result = LE_FAULT;
//         }
//     }
//
//     le_mem_Release(atReqRef);  // Release le_atClient_cmdsync_SetCommand
//     le_mem_Release(resRef);     // Release SendCommand
//
//     return result;








/*
    char command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char* interRespPtr   = "+CMGS:";
    const char* respPtr        = "OK|ERROR|+CME ERROR:|+CMS ERROR:";

    char* tokenPtr              = NULL;
    char* rest                  = NULL;
    char* savePtr               = NULL;
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t res;
    int32_t result;
    char intermediateResponse[LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES];
    char finalResponse[LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES];

    if (!dataPtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+CMGS=%d",length);

    // TODO: check the smsc field !
    hexStringSize = le_hex_BinaryToString(dataPtr,length+1,hexString,sizeof(hexString));

    cmdRef = le_atClient_Create();
    LE_DEBUG("New command ref (%p) created",cmdRef);
    if(cmdRef == NULL)
    {
        return;
    }
    res = le_atClient_SetCommand(cmdRef,command);
    if (res != LE_OK)
    {
        le_atClient_Delete(cmdRef);
        LE_ERROR("Failed to set the command !");
        return;
    }
    res = le_atClient_SetData(cmdRef,hexString,hexStringSize);
    if (res != LE_OK)
    {
        le_atClient_Delete(cmdRef);
        LE_ERROR("Failed to set data !");
        return;
    }
    res = le_atClient_SetIntermediateResponse(cmdRef,interRespPtr);
    if (res != LE_OK)
    {
        le_atClient_Delete(cmdRef);
        LE_ERROR("Failed to set intermediate response !");
        return;
    }
    res = le_atClient_SetFinalResponse(cmdRef,respPtr);
    if (res != LE_OK)
    {
        le_atClient_Delete(cmdRef);
        LE_ERROR("Failed to set final response !");
        return;
    }
    res = le_atClient_Send(cmdRef);
    if (res != LE_OK)
    {
        le_atClient_Delete(cmdRef);
        LE_ERROR("Failed to send !");
        return;
    }


    if (res != LE_OK)
    {
        le_atClient_Delete(cmdRef);
        return;
    }
    else
    {
        res = le_atClient_GetFinalResponse(cmdRef,finalResponse,LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES);
        if ((res != LE_OK) || (strcmp(finalResponse,"OK") != 0))
        {
            LE_ERROR("Failed to get the response");
            le_atClient_Delete(cmdRef);
            return;
        }

        res = le_atClient_GetFirstIntermediateResponse(cmdRef,intermediateResponse,LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES);
        if (res != LE_OK)
        {
            LE_ERROR("Failed to get the response");
            le_atClient_Delete(cmdRef);
            return;
        }

    }

    le_atClient_Delete(cmdRef);
    return result;*/

    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the message from the preferred message storage.
 *
 * @return LE_FAULT        The function failed to get the message from the preferred message
 *                         storage.
 * @return LE_TIMEOUT      No response was received from the Modem.
 * @return LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_RdPDUMsgFromMem
(
    uint32_t            index,      ///< [IN] The place of storage in memory.
    pa_sms_Protocol_t   protocol,   ///< [IN] The protocol used.
    pa_sms_Storage_t    storage,    ///< [IN] SMS Storage used.
    pa_sms_Pdu_t*       msgPtr      ///< [OUT] The message.
)
{
//     int32_t result = LE_FAULT;
//     le_atClient_CmdRef_t atReqRef;
//     le_atClient_CmdSyncResultRef_t  resRef = NULL;
//     char atcommand[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
//     const char* interRespPtr[] = {"+CMGR:",NULL};
//     const char* finalRespOkPtr[] = {"OK",NULL};
//     const char* finalRespKoPtr[] = {"ERROR","+CME ERROR:","+CMS ERROR:","TIMEOUT",NULL};
//
//     if (!msgPtr)
//     {
//         LE_ERROR("One parameter is NULL");
//         return LE_BAD_PARAMETER;
//     }
//
//     snprintf(atcommand,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"at+cmgr=%d",index);
//
//     atReqRef = CreateCmd();
//     AddCommand(atReqRef,atcommand,true);
//     AddData(atReqRef,NULL,0);
//     SetTimer(atReqRef,30000,GetTimerExpiryHandler());
//     AddIntermediateResp(atReqRef,GetIntermediateEventId(),interRespPtr);
//     AddFinalResp(atReqRef,GetFinalEventId(),finalRespOkPtr);
//     AddFinalResp(atReqRef,GetFinalEventId(),finalRespKoPtr);
//
//     resRef = SendCommand(AllPorts[ATPORT_COMMAND],atReqRef);
//
//     result = le_atClient_CheckCommandResult(resRef,finalRespOkPtr,finalRespKoPtr);
//     if ( result != LE_OK )
//     {
//         le_mem_Release(atReqRef);  // Release le_atClient_cmdsync_SetCommand
//         le_mem_Release(resRef);     // Release SendCommand
//         return result;
//     }
//
//     // there should be 3 line:
//     // first : +CMGR: ...
//     // second:  pdu data
//     if (le_atClient_GetNumLines(resRef) == 3)
//     {
//         // it parse just the first line because of '\0'
//         char* line = GetLine(resRef,0);
//         pa_utils_CountAndIsolateLineParameters(line);
//
//         // Check is the +CMGR intermediate response is in good format
//         if (FIND_STRING("+CMGR:",pa_utils_IsolateLineParameter(line,1)))
//         {
//             const char* pduPtr = GetLine(resRef,1);
//
//             msgPtr->status=(le_sms_Status_t)atoi(pa_utils_IsolateLineParameter(line,2));
//
//             int32_t dataSize = le_hex_StringToBinary(pduPtr,strlen(pduPtr),msgPtr->data,LE_SMS_PDU_MAX_BYTES);
//             if ( dataSize < 0)
//             {
//                 LE_ERROR("Message cannot be converted");
//                 result = LE_FAULT;
//             }
//             else
//             {
//                 LE_DEBUG("Fill message in binary mode");
//                 msgPtr->dataLen = dataSize;
//                 result = LE_OK;
//             }
//         } else {
//             LE_WARN("this pattern is not expected");
//             result = LE_FAULT;
//         }
//     } else {
//         LE_WARN("this pattern is not expected");
//         result = LE_FAULT;
//     }
//
//     le_mem_Release(resRef);     // Release SendCommand
//     le_mem_Release(atReqRef);   // Release CreateCmd
//
//     return result;
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the indexes of messages stored in the preferred memory for a specific
 * status.
 *
 * @return LE_FAULT          The function failed to get the indexes of messages stored in the
 *                           preferred memory.
 * @return LE_BAD_PARAMETER  The parameters are invalid.
 * @return LE_TIMEOUT        No response was received from the Modem.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_ListMsgFromMem
(
    le_sms_Status_t     status,     ///< [IN] The status of message in memory.
    pa_sms_Protocol_t   protocol,   ///< [IN] The protocol to read.
    uint32_t           *numPtr,     ///< [OUT] The number of indexes retrieved.
    uint32_t           *idxPtr,     ///< [OUT] The pointer to an array of indexes.
                                    ///        The array is filled with 'num' index values.
    pa_sms_Storage_t    storage     ///< [IN] SMS Storage used.
)
{
    char                 command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char*          interRespPtr = "+CMGL:";
    const char*          respPtr      = "\0";
    char*                tokenPtr     = NULL;
    char*                rest         = NULL;
    char*                savePtr      = NULL;
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;
    char                 response[LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES];
    char                 finalResponse[LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES];

    // TODO: storage option to manage

    if (!numPtr && !idxPtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+CMGL=%d",status);

    res = le_atClient_SetCommandAndSend(&cmdRef,command,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    if (res == LE_OK)
    {
        res = le_atClient_GetFinalResponse(cmdRef,finalResponse,LE_ATCLIENT_RESPLINE_SIZE_MAX_BYTES);
        if (res != LE_OK)
        {
            LE_ERROR("Failed to get the response");
            le_atClient_Delete(cmdRef);
            return res;
        }

        res = le_atClient_GetFirstIntermediateResponse(cmdRef,response,50);

        // CODE A REVOIR
        uint32_t cpt = 0;
        rest = response+strlen("+CMGL: ");
        tokenPtr = strtok_r(rest, ",", &savePtr);
        idxPtr[cpt]=atoi(tokenPtr);


        while(strcmp(response,"OK") != 0)
        {
            (*numPtr)++;
            cpt += 1;
            le_atClient_GetNextIntermediateResponse(cmdRef,response,50);
            rest = response+strlen("+CMGL: ");
            tokenPtr = strtok_r(rest, ",", &savePtr);
            idxPtr[cpt]=atoi(tokenPtr);
        }
    }

    le_atClient_Delete(cmdRef);
    return res;




//     size_t numberOfLine = le_atClient_GetNumLines(resRef);
//     for (cpt=0,*numPtr=0;cpt<numberOfLine;cpt++)
//     {
//         char* line = GetLine(resRef,cpt);
//         // it parse just the first line because of '\0'
//         uint32_t  numParam = pa_utils_CountAndIsolateLineParameters(line);
//
//         if (FIND_STRING("OK",pa_utils_IsolateLineParameter(line,1)))
//         {
//             result = LE_OK;
//             break;
//         }
//         else
//         {
//         // Check is the +CMGL intermediate response is in good format
//             if ((numParam>2) && (FIND_STRING("+CMGL:",pa_utils_IsolateLineParameter(line,1))))
//             {
//                 idxPtr[cpt]=atoi(pa_utils_IsolateLineParameter(line,2));
//                 (*numPtr)++;
//             }
//             else
//             {
//                 LE_WARN("this pattern is not expected");
//                 result = LE_FAULT;
//                 break;
//             }
//         }
//     }
//
//     le_mem_Release(resRef);     // Release SendCommand
//
//     return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function deletes one specific Message from preferred message storage.
 *
 * @return LE_FAULT          The function failed to delete one specific Message from preferred
 *                           message storage.
 * @return LE_TIMEOUT        No response was received from the Modem.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_DelMsgFromMem
(
    uint32_t            index,    ///< [IN] Index of the message to be deleted.
    pa_sms_Protocol_t   protocol, ///< [IN] protocol.
    pa_sms_Storage_t    storage   ///< [IN] SMS Storage used..
)
{
    char                 command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+CMGD=%d,0",index);

    res = le_atClient_SetCommandAndSend(&cmdRef,command,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function deletes all Messages from preferred message storage.
 *
 * @return LE_FAULT        The function failed to delete all Messages from preferred message storage.
 * @return LE_TIMEOUT      No response was received from the Modem.
 * @return LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_DelAllMsg
(
    void
)
{
    // TODO: storage option to manage

    const char*          commandPtr   = "AT+CMGD=0,4";
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    res = le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function saves the SMS Settings.
 *
 * @return LE_FAULT        The function failed to save the SMS Settings.
 * @return LE_TIMEOUT      No response was received from the Modem.
 * @return LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_SaveSettings
(
    void
)
{
    const char*          commandPtr   = "AT+CSAS";
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    res = le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function restores the SMS Settings.
 *
 * @return LE_FAULT        The function failed to restore the SMS Settings.
 * @return LE_TIMEOUT      No response was received from the Modem.
 * @return LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_RestoreSettings
(
    void
)
{
    const char*          commandPtr   = "AT+CRES";
     const char*         interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    res = le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function changes the message status.
 *
 * @return LE_FAULT        The function failed to change the message status.
 * @return LE_TIMEOUT      No response was received from the Modem.
 * @return LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_ChangeMessageStatus
(
    uint32_t            index,    ///< [IN] Index of the message to be deleted.
    pa_sms_Protocol_t   protocol, ///< [IN] protocol.
    le_sms_Status_t     status,   ///< [IN] The status of message in memory.
    pa_sms_Storage_t    storage   ///< [IN] SMS Storage used..
)
{
    // TODO: storage option to manage

    char                 command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "\0";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    switch (status)
    {
        case LE_SMS_RX_READ:
            status = 1;
            break;
        case LE_SMS_RX_UNREAD:
            status = 0;
            break;
        case LE_SMS_STORED_SENT:
            status = 3;
            break;
        case LE_SMS_STORED_UNSENT:
            status = 2;
            break;
        default:
            return LE_FAULT;
    }

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+WMSC=%d,%d", index, status);

    res = le_atClient_SetCommandAndSend(&cmdRef,command,interRespPtr,respPtr,DEFAULT_AT_CMD_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the SMS center.
 *
 * @return
 *   - LE_FAULT        The function failed.
 *   - LE_TIMEOUT      No response was received from the Modem.
 *   - LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_GetSmsc
(
    char*        smscPtr,  ///< [OUT] The Short message service center string.
    size_t       len       ///< [IN] The length of SMSC string.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the SMS center.
 *
 * @return
 *   - LE_FAULT        The function failed.
 *   - LE_TIMEOUT      No response was received from the Modem.
 *   - LE_OK           The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_SetSmsc
(
    const char*    smscPtr  ///< [IN] The Short message service center.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Activate Cell Broadcast message notification
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_ActivateCellBroadcast
(
    pa_sms_Protocol_t protocol
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deactivate Cell Broadcast message notification
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_DeactivateCellBroadcast
(
    pa_sms_Protocol_t protocol
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add Cell Broadcast message Identifiers range.
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_AddCellBroadcastIds
(
    uint16_t fromId,    ///< [IN] Starting point of the range of cell broadcast message identifier.
    uint16_t toId       ///< [IN] Ending point of the range of cell broadcast message identifier.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove Cell Broadcast message Identifiers range.
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_RemoveCellBroadcastIds
(
    uint16_t fromId,    ///< [IN] Starting point of the range of cell broadcast message identifier.
    uint16_t toId       ///< [IN] Ending point of the range of cell broadcast message identifier.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add CDMA Cell Broadcast category services.
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_AddCdmaCellBroadcastServices
(
    le_sms_CdmaServiceCat_t serviceCat, ///< [IN] Service category assignment. Reference to 3GPP2 C.R1001-D
                                        ///       v1.0 Section 9.3.1 Standard Service Category Assignments.
    le_sms_Languages_t language         ///< [IN] Language Indicator. Reference to
                                        ///       3GPP2 C.R1001-D v1.0 Section 9.2.1 Language Indicator
                                        ///       Value Assignments
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove CDMA Cell Broadcast category services.
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_RemoveCdmaCellBroadcastServices
(
    le_sms_CdmaServiceCat_t serviceCat, ///< [IN] Service category assignment. Reference to 3GPP2 C.R1001-D
                                        /// v1.0 Section 9.3.1 Standard Service Category Assignments.
    le_sms_Languages_t language         ///< [IN] Language Indicator. Reference to
                                        ///       3GPP2 C.R1001-D v1.0 Section 9.2.1 Language Indicator
                                        ///       Value Assignments
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clear Cell Broadcast message Identifiers range.
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_ClearCellBroadcastIds
(
    void
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clear CDMA Cell Broadcast category services.
 *
 * @return
 *  - LE_FAULT         Function failed.
 *  - LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sms_ClearCdmaCellBroadcastServices
(
    void
)
{
    return LE_FAULT;
}
