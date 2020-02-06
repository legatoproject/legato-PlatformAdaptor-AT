/** @file pa_mdc_utils.c
 *
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"


#include "pa_utils.h"
#include "pa_mdc.h"
#include "pa_mdc_local.h"
#include "pa_mdc_utils_local.h"


//--------------------------------------------------------------------------------------------------
/**
 * Count myChar occurence in the inputString string
 *
 */
//--------------------------------------------------------------------------------------------------
static int CountMyChar
(
    const char* inputStringStr,     ///< [IN] IP address
    char myChar                     ///< [IN] Char to count
)
{
    int i, j, len;

    if(NULL == inputStringStr)
    {
        LE_ERROR("String null");
        return -1;
    }

    len = strnlen(inputStringStr, LE_ATDEFS_COMMAND_MAX_BYTES);
    for(i=0, j=0; i < len; i++)
    {
        if(myChar == (char) (inputStringStr[i]))
        {
            j++;
        }
    }

    return j;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set IPV6 string format
 *
 * @return LE_OK            Ipv6 string format set
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         Command failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_utils_setIPV6StringFormat
(
    void
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res;

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CGPIAF=0,0,0,0",
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
 * Attach or detach from the Packet Domain service.
 *
 * @return LE_OK            modem is attached or detached from the Packet domain service
 * @return LE_FAULT         modem could not attach or detach from the Packet domain service
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_utils_AttachPS
(
    bool toAttach               ///< [IN] boolean value
)
{
    le_atClient_CmdRef_t    cmdRef = NULL;
    le_result_t             res    = LE_FAULT;
    char                    responseStr[PA_AT_LOCAL_SHORT_SIZE] = {0};

    res = le_atClient_SetCommandAndSend(&cmdRef,
                        pa_utils_GetAtDeviceRef(),
                        (toAttach ? "AT+CGATT=1" : "AT+CGATT=0"),
                        DEFAULT_EMPTY_INTERMEDIATE,
                        DEFAULT_AT_RESPONSE,
                        DEFAULT_AT_CMD_TIMEOUT);

    if (LE_OK != res)
    {
        return LE_FAULT;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_SHORT_SIZE);
    le_atClient_Delete(cmdRef);

    if ((res != LE_OK) || (strcmp(responseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the final response : %s", responseStr);
        return LE_FAULT;
    }

    return res;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to retrieve the PDP type from PDP Context
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_utils_GetPDPType
(
    uint32_t profileIndex,               ///< [IN]  The profile to use
    le_mdc_Pdp_t *pdpTypePtr             ///< [OUT] The PDP Type
)
{
    le_atClient_CmdRef_t cmdRef   = NULL;
    le_result_t          res      = LE_FAULT;
    static const  char   cgdcontStr[]="+CGDCONT: ";
    char*                tokenPtr = NULL;
    char*                savePtr  = NULL;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};

    if ((!profileIndex) || (!pdpTypePtr))
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    snprintf(responseStr, PA_AT_LOCAL_SHORT_SIZE, "%s%d,",cgdcontStr, (int) profileIndex);

    res = le_atClient_SetCommandAndSend(&cmdRef,
        pa_utils_GetAtDeviceRef(),
        "AT+CGDCONT?",
        responseStr,
        DEFAULT_AT_RESPONSE,
        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }
    responseStr[0] = NULL_CHAR;
    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_STRING_SIZE);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the final response");
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

    // Default PDP type
    *pdpTypePtr = LE_MDC_PDP_IPV4;

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the intermediate response");
    }
    else
    {
        //  Look for APN string
        //  AT+CGDCONT?
        //  +CGDCONT: 1,"<pdp type>","<apn>",,0,0,0,0,0,,0)
        //  +CGDCONT: 1,"IPV4V6",,,0,0,0,0,0,,0
        //  +CGDCONT: 2,"IPV4V6","VZWADMIN",,0,0,0,0,0,,0
        //  Look for the first <">
        res = LE_FAULT;
        tokenPtr = strtok_r(responseStr, "\"", &savePtr);
        if(tokenPtr)
        {
            LE_DEBUG("tokenPtr1 %s", tokenPtr);
            tokenPtr = strtok_r(NULL, "\"", &savePtr);
            if(tokenPtr)
            {
                LE_DEBUG("tokenPtr2 %s", tokenPtr);
                if ( 0 == strcmp(tokenPtr, "IPV4V6") )
                {
                    *pdpTypePtr = LE_MDC_PDP_IPV4V6;
                    res = LE_OK;
                }
                else if( 0 == strcmp(tokenPtr, "IPV6") )
                {
                    *pdpTypePtr = LE_MDC_PDP_IPV6;
                    res = LE_OK;
                }
                else if( 0 == strcmp(tokenPtr, "IP") )
                {
                    *pdpTypePtr = LE_MDC_PDP_IPV4;
                    res = LE_OK;
                }
            }
        }
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function return true if profileIndex PDP context is activated
 *
 */
//--------------------------------------------------------------------------------------------------
bool pa_mdc_utils_IsConnected
(
    uint32_t profileIndex           ///< [IN]  The profile to use
)
{
    le_mdc_ConState_t sessionStatePtr = LE_MDC_DISCONNECTED;
    pa_mdc_GetSessionState(profileIndex, &sessionStatePtr);

    if(LE_MDC_CONNECTED == sessionStatePtr)
    {
        return true;
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function convert IPV6 dot format address to Hexa format
 *
 * @return LE_OK            The function succeeded.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ConvertIpv6AddrDotToHexaFormat
(
    char * ipv6DotAddress       ///< [IN/OUT] IPv6 address Dot -> to Hexa
)
{
    int i, index, value1, value2;
    char ipv6AddrLoc[PA_MDC_LOCAL_STRING_SIZE]=  {0};
    char bufferTempStr[20];

    // 254.128.0.0.0.0.0.0.0.0.0.0.0.0.0.1
    pa_utils_RemoveQuotationString(ipv6DotAddress);
    uint32_t numParam = pa_utils_CountAndIsolateLineParametersWithChar(ipv6DotAddress, DOT_CHAR);
    if (numParam != (NB_DOT_IPV6_ADDR+1))
    {
        LE_ERROR("Bad format");
        return LE_FAULT;
    }

    for(i=0; i<(numParam/2); i++)
    {
        index = (i*2)+1;
        // (i=0) => index = 1
        // (i=1) => index = 3
        value1 = atoi(pa_utils_IsolateLineParameter(ipv6DotAddress, index));
        value2 = atoi(pa_utils_IsolateLineParameter(ipv6DotAddress, index+1));
        snprintf(bufferTempStr, sizeof(ipv6AddrLoc), "%02X%02X", value1, value2);
        strncat(ipv6AddrLoc, bufferTempStr, sizeof(ipv6AddrLoc)-1);
        if(i < 7)
        {
            strncat(ipv6AddrLoc, ":", sizeof(ipv6AddrLoc)-1);
        }
    }

    LE_DEBUG("Converions done %s", ipv6AddrLoc);
    strcpy(ipv6DotAddress, ipv6AddrLoc);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function check IPv4/IPv6 string format. Convert DOT IPV6 address to hex format if detected
 *
 * @return true         The inputStringStr string is a ipVersion type
 * @return false        The inputStringStr doesn't match with the ipVersion type.
 *
 */
//--------------------------------------------------------------------------------------------------
bool pa_mdc_util_CheckConvertIPAddressFormat
(
    char* ipAddressStr,                       ///< [IN/OUT] IP address
    le_mdmDefs_IpVersion_t  ipVersion         ///< [IN] IP Version
)
{
    int nbdot;

    if(!ipAddressStr)
    {
        return false;
    }

    nbdot = CountMyChar(ipAddressStr, DOT_CHAR);
    if(LE_MDMDEFS_IPV4 == ipVersion)
    {
        if(NB_DOT_IPV4_ADDR == nbdot)
        {
            return true;
        }
    }
    else if(LE_MDMDEFS_IPV6 == ipVersion)
    {
        if(NB_DOT_IPV6_ADDR == nbdot)
        {
            if( LE_OK == ConvertIpv6AddrDotToHexaFormat(ipAddressStr))
            {
                return true;
            }
        } else if(CountMyChar(ipAddressStr, COLONS_CHAR) >= 2)
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function return the gateway address from +CGCONTRDP response.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_util_GetGWAddr
(
    const char*  inputStringStr,    ///< [IN] Input string to parse
    char*  gwOutput,                ///< [OUT] GW address string
    size_t gwOutputLen              ///< [IN] GW buffer size
)
{
    char            gwLocalStr[PA_AT_LOCAL_STRING_SIZE] = {0};
    int             i, j;
    int             nbComma;
    int             len;

    /*
     * The execution command returns the relevant information <bearer_id>, <apn>, <ip_addr>, <subnet_mask>,
     *
     *  <gw_addr>, <DNS_prim_addr>, <DNS_sec_addr>, <P-CSCF_prim_addr>, <P-CSCF_sec_addr> and
     * <IM_CN_Signalling_Flag> for a non secondary PDP Context established by the network with the primary
     * context identifier <cid>. If the context cannot be found an ERROR response is returned.
     * If the MT has dual stack capabilities, two lines of information are returned per <cid>. First one line with the IPv4
     * parameters followed by one line with the IPv6 parameters.
     *
     */

    //+CGCONTRDP: <cid>,<bearer_id>,<apn>[,<source address and subnetmask>
    //    [,<gw_addr>[,<DNS_prim_addr>[,<DNS_sec_addr>
    //    [,<P-CSCF_prim_addr>[,<P-CSCF_sec_addr>[,<IM_CN_Signalling_Flag>]]]]]]]
    //    [<CR><LF>+CGCONTRDP: <cid>,<bearer_id>,<apn>[,<source address and subnet mask>
    //    [,<gw_addr>[,<DNS_prim_addr>[,<DNS_sec_addr>[,<P-CSCF_prim_addr>[,<P-CSCF_sec_addr>[,<IM_CN_Signalling_Flag>]]]]]]]
    //    [...]]
    //+CGCONTRDP: 1,5,"apn.fr",10.40.54.48.255.255.255.224,10.40.54.33,10.40.10.1,10.40.10.2,,,
    //+CGCONTRDP: 1,5,"cmw500.rohde-schwarz.com.mnc001.mcc001.gprs",254.128.0.0.0.0.0.0.0.0.0.0.0.0.0.1.255.255.255.255.255.255.255.255.255.255.255.255.255.255.255.255,,252.177.202.254.0.0.0.0.0.0.0.0.0.0.0.2,,,

    gwOutput[0] = NULL_CHAR;
    gwLocalStr[0] = NULL_CHAR;
    nbComma = CountMyChar(inputStringStr, COMMA_CHAR);
    len = strnlen(inputStringStr, LE_ATDEFS_COMMAND_MAX_BYTES);

    if (nbComma < 4)
    {
        LE_DUMP((uint8_t *) inputStringStr, len);
        LE_ERROR("Add Error");
        return LE_FAULT;
    }

    for(i=0, j=0; i<len ; i++)
    {
        if (',' == inputStringStr[i])
        {
            j++;
            if (4 == j)
            {
                strncpy(gwLocalStr,  &inputStringStr[i+1], sizeof(gwLocalStr));
                break;
            }
        }
    }

    if(j != 4)
    {
        LE_DUMP((uint8_t *) inputStringStr, len);
        LE_ERROR("Add Error");
        return LE_FAULT;
    }

    len = strnlen(gwLocalStr, sizeof(gwLocalStr));
    for(i=0; i<len; i++)
    {
        if (COMMA_CHAR == gwLocalStr[i])
        {
            gwLocalStr[i] = NULL_CHAR;
            break;
        }
    }
    strncpy(gwOutput, gwLocalStr, gwOutputLen);
    pa_utils_RemoveQuotationString(gwOutput);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function return the DNS1 and DNS2 addresses from +CGCONTRDP response.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_util_GetDNSAddr
(
    const char*  inputStringStr,    ///< [IN] Input string to parse
    char*  dns1Str,                 ///< [OUT] DNS1 address string
    size_t dns1StrLen,              ///< [IN] DNS1 buffer size
    char*  dns2Str,                 ///< [OUT] DNS2 address string
    size_t dns2StrLen               ///< [IN] DNS2 buffer size
)
{
    char            buffStr[PA_MDC_LOCAL_STRING_SIZE*2] = {0};
    int             i, j;
    int             nbComma;
    int             len;
    char *          ptr1;

    /*
     * The execution command returns the relevant information <bearer_id>, <apn>, <ip_addr>, <subnet_mask>,
     *
     *  <gw_addr>, <DNS_prim_addr>, <DNS_sec_addr>, <P-CSCF_prim_addr>, <P-CSCF_sec_addr> and
     * <IM_CN_Signalling_Flag> for a non secondary PDP Context established by the network with the primary
     * context identifier <cid>. If the context cannot be found an ERROR response is returned.
     * If the MT has dual stack capabilities, two lines of information are returned per <cid>. First one line with the IPv4
     * parameters followed by one line with the IPv6 parameters.
     *
     */

    //+CGCONTRDP: <cid>,<bearer_id>,<apn>[,<source address and subnetmask>
    //    [,<gw_addr>[,<DNS_prim_addr>[,<DNS_sec_addr>
    //    [,<P-CSCF_prim_addr>[,<P-CSCF_sec_addr>[,<IM_CN_Signalling_Flag>]]]]]]]
    //    [<CR><LF>+CGCONTRDP: <cid>,<bearer_id>,<apn>[,<source address and subnet mask>
    //    [,<gw_addr>[,<DNS_prim_addr>[,<DNS_sec_addr>[,<P-CSCF_prim_addr>[,<P-CSCF_sec_addr>[,<IM_CN_Signalling_Flag>]]]]]]]
    //    [...]]
    //+CGCONTRDP: 1,5,"apn.fr",10.40.54.48.255.255.255.224,10.40.54.33,10.40.10.1,10.40.10.2,,,
    //+CGCONTRDP: 1,5,"cmw500.rohde-schwarz.com.mnc001.mcc001.gprs",254.128.0.0.0.0.0.0.0.0.0.0.0.0.0.1.255.255.255.255.255.255.255.255.255.255.255.255.255.255.255.255,,252.177.202.254.0.0.0.0.0.0.0.0.0.0.0.2,,,

    dns1Str[0] = NULL_CHAR;
    dns2Str[0] = NULL_CHAR;

    nbComma = CountMyChar(inputStringStr, COMMA_CHAR);
    len = strnlen(inputStringStr, LE_ATDEFS_RESPONSE_MAX_BYTES);

    if (nbComma < 5)
    {
        LE_DUMP((uint8_t *) inputStringStr, len);
        LE_ERROR("error");
        return LE_FAULT;
    }

    for(i=0, j=0; i<len ; i++)
    {
        if (',' == inputStringStr[i])
        {
            j++;
            if (5 == j)
            {
                strncpy(buffStr,  &inputStringStr[i+1], sizeof(buffStr));
                break;
            }
        }
    }

    if(j != 5)
    {
        LE_DUMP((uint8_t *) inputStringStr, len);
        LE_ERROR("error");
        return LE_FAULT;
    }

    ptr1 = buffStr;
    len = strnlen(buffStr, sizeof(buffStr));

    for(i=0; i<len; i++)
    {
        if (COMMA_CHAR == buffStr[i])
        {
            buffStr[i] = NULL_CHAR;
            ptr1 = &buffStr[i+1];
            break;
        }
    }
    strncpy(dns1Str, buffStr, dns1StrLen);
    pa_utils_RemoveQuotationString(dns1Str);

    len = strnlen(ptr1, sizeof(buffStr));
    for(i=0; i<len; i++)
    {
        if (COMMA_CHAR == ptr1[i])
        {
            ptr1[i] = NULL_CHAR;
            break;
        }
    }
    strncpy(dns2Str, ptr1, dns2StrLen);
    pa_utils_RemoveQuotationString(dns2Str);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get autentification setting on PDP context
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_utils_GetAuth
(
    uint32_t                profileIndex,          ///< [IN]  The profile to use
    pa_mdc_ProfileData_t*   profileDataPtr
)
{
    le_atClient_CmdRef_t    cmdRef   = NULL;
    char                    responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};
    char*                   tokenPtr = NULL;
    char*                   savePtr  = NULL;
    char                    logStr[PA_MDC_LOCAL_STRING_SIZE] = {0};
    char                    pwdStr[PA_MDC_LOCAL_STRING_SIZE] = {0};
    int                     val = 0;
    le_result_t             res = LE_OK;

    if (!profileIndex)
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    /*
     * AT+CGAUTH? 3GPP At command
     * [+CGAUTH: <cid>,<auth_prot>,<userid>,<password>]
     * [<CR><LF>+CGAUTH: <cid>,<auth_prot>,<userid>,<password>
     * [...]]
     * +CGAUTH: 1,1,,
     * +CGAUTH: 2,1,,
     * +CGAUTH: 7,2,loging,passwd
     * OK
     * at+cgauth=?
     * +CGAUTH : (1 - 15),(NONE, PAP, CHAP),<username>,<password>
     * OK
     *
     */
    snprintf(responseStr, PA_AT_LOCAL_STRING_SIZE, "+CGAUTH: %"PRIu32, profileIndex);

    res = le_atClient_SetCommandAndSend(&cmdRef,
        pa_utils_GetAtDeviceRef(),
        "AT+CGAUTH?",
        responseStr,
        DEFAULT_AT_RESPONSE,
        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    responseStr[0] = NULL_CHAR;
    res = le_atClient_GetFinalResponse(cmdRef,
        responseStr,
        PA_AT_LOCAL_STRING_SIZE);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the final response");
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
        LE_WARN("Failed to get the intermediate response");
    }
    else
    {
        profileDataPtr->authentication.type = LE_MDC_AUTH_NONE;
        //  Look for the first <,>
        tokenPtr = strtok_r(responseStr + strlen("+CGAUTH: "), ",", &savePtr);
        if(tokenPtr)
        {
            LE_DEBUG("tokenPtr %s", tokenPtr);
            val = atoi(tokenPtr);
            if (val != profileIndex)
            {
                return LE_FAULT;
            }

            tokenPtr = strtok_r(NULL, ",", &savePtr);
            if(tokenPtr)
            {
                LE_DEBUG("tokenPtr %s", tokenPtr);
                val = atoi(tokenPtr);
                switch(val)
                {
                    case 0:
                        LE_DEBUG("Auth none");
                        profileDataPtr->authentication.type = LE_MDC_AUTH_NONE;
                        break;
                    case 1:
                        LE_DEBUG("Auth pap");
                        profileDataPtr->authentication.type = LE_MDC_AUTH_PAP;
                        break;
                    case 2:
                        LE_DEBUG("Auth chap");
                        profileDataPtr->authentication.type = LE_MDC_AUTH_CHAP;
                        break;
                    default:
                        LE_DEBUG("Auth Faild %d", val);
                        return LE_FAULT;
                }

                tokenPtr = strtok_r(NULL, ",", &savePtr);
                if(tokenPtr)
                {
                    LE_DEBUG("Log  %s", tokenPtr);
                    snprintf(logStr, PA_MDC_LOCAL_STRING_SIZE, "%s", tokenPtr);

                    tokenPtr = strtok_r(NULL, ",", &savePtr);
                    if(tokenPtr)
                    {
                        LE_DEBUG("Pwd %s", tokenPtr);
                        snprintf(pwdStr, PA_MDC_LOCAL_STRING_SIZE, "%s", tokenPtr);
                    }
                    else
                    {
                        LE_DEBUG("Pwd NULL");
                    }
                }
            }
        }
        pa_utils_RemoveQuotationString(logStr);
        pa_utils_RemoveQuotationString(pwdStr);
        strncpy(profileDataPtr->authentication.userName, logStr, PA_MDC_USERNAME_MAX_BYTES);
        strncpy(profileDataPtr->authentication.password, pwdStr, PA_MDC_PWD_MAX_BYTES);
    }

    return res;
}
