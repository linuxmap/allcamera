#include "svs_ace_header.h"
#include "svs_adapter_common.h"
#include "svs_log_msg.h"
#include <vms/vms.h>
#include <errno.h>


//���ñ��ش洢���豸ID�ֶ�,������ʧ�ܻ�ض�ʱֻ��ӡ��������
void M_COMMON::SetLocalizeDeviceId(uint8_t *pSetPos,
                    const uint32_t uiSetSize,
                    const char *pszDeviceId,
                    const uint32_t uiStrictLen)
{
    if (NULL == pSetPos || NULL == pszDeviceId || 0 == uiSetSize)
    {
        SVS_LOG((SVS_LM_WARNING, "Set localize device id failed, "
            "pSetPos[%p], pszDeviceId[%p], setSize[%d].",
            pSetPos, pszDeviceId, uiSetSize));

        return ;
    }

    memset(pSetPos, 0x0, uiSetSize);

    //���ڱ��ش洢�����洢�ռ�С��32,���ӡ�澯��Ϣ
    if (uiSetSize <= LOCALIZE_DEVICE_ID_MAX_LEN)
    {
        SVS_LOG((SVS_LM_WARNING, "Set localize device id illegality, "
            "setSize[%d] less than %d.",
            uiSetSize, LOCALIZE_DEVICE_ID_MAX_LEN));
    }

    //���ָ���˳��ȣ���ָ�����ȿ���
    if (0 != uiStrictLen)
    {
        uint32_t uiCopyLen = LOCALIZE_DEVICE_ID_MAX_LEN;

        //���ָ���Ŀ������ȴ�����󿽱����ȣ����ӡ�澯��Ϣ
        if (uiStrictLen > uiCopyLen)
        {
            SVS_LOG((SVS_LM_WARNING, "Set localize device id illegality, "
                "strictLen[%d] max than %d.",
                uiStrictLen, LOCALIZE_DEVICE_ID_MAX_LEN));
        }
        else
        {
            uiCopyLen = uiStrictLen;
        }

        //�����趨���ȴ������紫�䳤��Ҳ��ӡ�澯��־
        if (uiCopyLen > NLS_DEVICE_ID_MAX_LEN)
        {
            SVS_LOG((SVS_LM_WARNING, "Set localize device id warning, "
                "idLen[%d] max than %d.",
                uiCopyLen, NLS_DEVICE_ID_MAX_LEN));
        }

        memcpy(pSetPos, pszDeviceId, uiCopyLen);
    }
    //��ȡ�ַ�ĳ���
    else
    {
        uint32_t uiCopyLen = LOCALIZE_DEVICE_ID_MAX_LEN;

        //�����ַ��ȴ��ڿɿ�������󳤶ȵģ���ӡ������־
        uint32_t strLen = strlen(pszDeviceId);
        if (strLen > uiCopyLen)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Set localize device id illegality, idLen[%u] max than %d.",
                strLen, LOCALIZE_DEVICE_ID_MAX_LEN));
        }
        else
        {
            uiCopyLen = strlen(pszDeviceId);
        }

        //�����趨���ȴ������紫�䳤��Ҳ��ӡ�澯��־
        if (uiCopyLen > NLS_DEVICE_ID_MAX_LEN)
        {
            SVS_LOG((SVS_LM_WARNING, "Set localize device id warning, "
                "idLen[%d] max than %d.",
                uiCopyLen, NLS_DEVICE_ID_MAX_LEN));
        }

        memcpy(pSetPos, pszDeviceId, uiCopyLen);
    }
}

void M_COMMON::SetNLSDeviceId(uint8_t *pSetPos,
                    const uint32_t uiSetSize,
                    const char *pszDeviceId,
                    const uint32_t uiStrictLen)
{
    if (NULL == pSetPos || NULL == pszDeviceId || 0 == uiSetSize)
    {
        SVS_LOG((SVS_LM_WARNING, "Set NLS device id failed, "
            "pSetPos[%p], pszDeviceId[%p], setSize[%d].",
            pSetPos, pszDeviceId, uiSetSize));

        return ;
    }

    memset(pSetPos, 0x0, uiSetSize);

    //���ڱ��ش洢�����洢�ռ�С��32,���ӡ�澯��Ϣ
    if (uiSetSize < NLS_DEVICE_ID_MAX_LEN)
    {
        SVS_LOG((SVS_LM_WARNING, "Set NLS device id illegality, "
            "setSize[%d] less than %d.",
            uiSetSize, NLS_DEVICE_ID_MAX_LEN));
    }

    //���ָ���˳��ȣ���ָ�����ȿ���
    if (0 != uiStrictLen)
    {
        uint32_t uiCopyLen = NLS_DEVICE_ID_MAX_LEN;

        //���ָ���Ŀ������ȴ�����󿽱����ȣ����ӡ�澯��Ϣ
        if (uiStrictLen > uiCopyLen)
        {
            SVS_LOG((SVS_LM_WARNING, "Set NLS device id illegality, "
                "strictLen[%d] max than %d.",
                uiStrictLen, NLS_DEVICE_ID_MAX_LEN));
        }
        else
        {
            uiCopyLen = uiStrictLen;
        }

        //�����趨���ȴ������紫�䳤��Ҳ��ӡ�澯��־
        if (uiCopyLen > NLS_DEVICE_ID_MAX_LEN)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Set NLS device id warning, idLen[%u] max than %d.",
                uiCopyLen, NLS_DEVICE_ID_MAX_LEN));
        }

        memcpy(pSetPos, pszDeviceId, uiCopyLen);
    }
    //��ȡ�ַ�ĳ���
    else
    {
        uint32_t uiCopyLen = NLS_DEVICE_ID_MAX_LEN;

        //�����ַ��ȴ��ڿɿ�������󳤶ȵģ���ӡ������־
        uint32_t strLen = strlen(pszDeviceId);
        if (strLen > uiCopyLen)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Set NLS device id illegality, idLen[%u] max than %d.",
                strLen, NLS_DEVICE_ID_MAX_LEN));
        }
        else
        {
            uiCopyLen = strlen(pszDeviceId);
        }

        //�����趨���ȴ������紫�䳤��Ҳ��ӡ�澯��־
        if (uiCopyLen > NLS_DEVICE_ID_MAX_LEN)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Set NLS device id warning, idLen[%u] max than %d.",
                uiCopyLen, NLS_DEVICE_ID_MAX_LEN));
        }

        memcpy(pSetPos, pszDeviceId, uiCopyLen);
    }
}

void M_COMMON::FillCommonHeader(PSVS_MSG_HEADER pHdr,
                                        uint32_t MsgType,
                                        const char * strMuID,
                                        uint32_t TransactionNo,
                                        uint32_t PacketLength)
{
    pHdr->ProtocolVersion = SVS_MSG_PROTOCOL_VERSION;
    pHdr->MsgType = MsgType;
    pHdr->TransactionNo = TransactionNo;
    pHdr->PacketLength = PacketLength;
}

int32_t M_COMMON::digit_check(const char * str)
{
    if (NULL == str)
    {
        return -1;
    }

    while ('\0' != *str)
    {
        if (0 == isalnum(*str))
        {
            return -1;
        }

        ++str;
    }

    return 0;
}
// �ܶ���վ��ID�ĵط�Ҳ�����˴˺���������֮
int32_t M_COMMON::CheckDeviceID(const char *pszDevID)
{
    if (NULL == pszDevID)
    {
        return CHECK_ERROR;
    }

    uint32_t iDeviceLen = strlen(pszDevID);

    if (0 == iDeviceLen || iDeviceLen > DEVICE_ID_LEN)
    {
        return CHECK_ERROR;
    }

    for (uint32_t i = 0; i < iDeviceLen; i++)
    {
        if((pszDevID[i] >= '0' && pszDevID[i] <= '9')
            || (pszDevID[i] >= 'a' && pszDevID[i] <= 'z')
            || (pszDevID[i] >= 'A' && pszDevID[i] <= 'Z'))
        {
            continue;
        }
        else
        {
            return CHECK_ERROR;
        }
    }

    return CHECK_OK;
}

int32_t M_COMMON::CheckDeviceIDEx(const char *pszDevID)
{
    if (NULL == pszDevID)
    {
        return CHECK_ERROR;
    }

    uint32_t iDeviceLen = strlen(pszDevID);

    // �ϰ汾�淶18��20λ���ƶ�15λ
    if ((iDeviceLen != 18) && (iDeviceLen != 20) && (iDeviceLen != 15))
    {
        return CHECK_ERROR;
    }

    for (uint32_t i = 0; i < iDeviceLen; i++)
    {
        if((pszDevID[i] >= '0' && pszDevID[i] <= '9'))
        {
            continue;
        }
        else
        {
            return CHECK_ERROR;
        }
    }

    return CHECK_OK;
}

bool M_COMMON::IsSockErrorOk(int32_t iErrorCode)
{
    if (EAGAIN == iErrorCode
        || ETIME == iErrorCode
        || EWOULDBLOCK == iErrorCode
        || ETIMEDOUT == iErrorCode
        || EINTR == iErrorCode)
    {
        return true;
    }

    return false;
}

/**
* Description: ɾ���ַ�ǰ��Ŀ��ַ�
* @param [inout]   srcString:   �������ַ�
* @return ��
*/
void M_COMMON::trimString(std::string& srcString)
{
    string::size_type pos = srcString.find_last_not_of(' ');
    if (pos != string::npos)
    {
        (void) srcString.erase(pos + 1);
        pos = srcString.find_first_not_of(' ');
        if (pos != string::npos)
            (void) srcString.erase(0, pos);
    }
    else
        (void) srcString.erase(srcString.begin(), srcString.end());

    return;
}
