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
 * This function retrieves the identifier for the embedded Universal Integrated Circuit Card (EID)
 * (16 digits)
 *
 * @return LE_OK            The function succeeded.
 * @return LE_FAULT         The function failed.
 * @return LE_UNSUPPORTED   The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetCardEID
(
   pa_sim_Eid_t eid               ///< [OUT] the EID value
)
{
    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
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
le_result_t pa_sim_GetPINRemainingAttempts
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
le_result_t pa_sim_GetPUKRemainingAttempts
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
le_result_t pa_sim_ChangePIN
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
le_result_t pa_sim_EnablePIN
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
le_result_t pa_sim_DisablePIN
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
 * This function must be called to get the Home Network Name information.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the Home Network Name can't fit in nameStr
 *      - LE_FAULT on any other failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_GetHomeNetworkOperator
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
le_result_t pa_sim_GetHomeNetworkMccMnc
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
 * This function must be called to trigger a SIM refresh.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT for unexpected error
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_sim_Refresh
(
    void
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
le_result_t pa_sim_ConfirmSimToolkitCommand
(
    bool  confirmation  ///< [IN] true to accept, false to reject
)
{
    return LE_FAULT;
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
le_result_t pa_sim_EnterPUK
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
