#ifndef _SVS_Sub_System_h
#define _SVS_Sub_System_h

#include <string>
#include <vms/vms.h>
#include "svs_adapter_vms_rtp.h"
using namespace std;

//���ڱ��ش洢���豸ID��󳤶�
#define LOCALIZE_DEVICE_ID_MAX_LEN                 32
//�������紫����豸ID��󳤶�
#define NLS_DEVICE_ID_MAX_LEN                       20

#define SVS_SERVER_NAME_LEN                         128


#define CHECK_OK     0
#define CHECK_ERROR -1

#define MPEG_STARTCODE                ((int32_t)0x000001)
#define MPEG_STARTCODE_SIZE           3
// new��delete��װ
enum ENUM_M_DELETE_MULTI
{
    M_DELETE_SINGLE = 0,
    M_DELETE_MULTI = 1
};

template<class T>
T* M_NEW(T* &m, uint32_t nMuili = M_DELETE_SINGLE)
{
    try
    {
        if ( M_DELETE_SINGLE == nMuili )
        {
            m = new(T);
        }
        else
        {
            m = new T[nMuili];
        }
    }
    catch(...)
    {
        m = NULL;
        return NULL;
    }
    return m;
};

template<class T>
void M_DELETE(T* &m, uint32_t nMuili = M_DELETE_SINGLE)
{
    if(NULL == m)
    {
        return;
    }
    try
    {
        if (M_DELETE_SINGLE == nMuili)
        {
            delete m;
        }
        else
        {
            delete[] m;
        }
    }
    catch(...)
    {
    }

    m = NULL;
    return;
};

namespace M_COMMON
{
    void SetLocalizeDeviceId(uint8_t *pSetPos,
                        const uint32_t uiSetSize,
                        const char *pszDeviceId,
                        const uint32_t uiStrictLen = 0);

    void SetNLSDeviceId(uint8_t *pSetPos,
                    const uint32_t uiSetSize,
                    const char *pszDeviceId,
                    const uint32_t uiStrictLen = 0);
    void FillCommonHeader(PSVS_MSG_HEADER pHdr,
                                uint32_t MsgType,
                                const char* strMuID,
                                uint32_t TransactionNo,
                                uint32_t PacketLength);

    inline uint32_t transactionno_respond(uint16_t &transactionno)
    {
        transactionno |= 0x8000;
        return transactionno;
    }

    int32_t digit_check(const char* str);

    int32_t CheckDeviceID(const char * pszDevID);

    int32_t CheckDeviceIDEx(const char * pszDevID);

    // ���errno�ж�socket�Ƿ���
    bool IsSockErrorOk(int32_t iErrorCode);

    /**
    * Description: ɾ���ַ�ǰ��Ŀ��ַ�
    * @param [inout]   srcString:   �������ַ�
    * @return ��
    */
    void trimString(std::string& srcString);


}// namespace M_COMMON
#endif //
