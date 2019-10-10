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
        strncat(ipv6AddrLoc, bufferTempStr, sizeof(ipv6AddrLoc));
        if(i < 7)
        {
            strncat(ipv6AddrLoc, ":", sizeof(ipv6AddrLoc));
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
