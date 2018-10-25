/////////////////////////////////////////////////////////////////////
//    All rights reserved.
//
//    Author        : Stone Shi
//    Description    : ʵ�ú���ͷ�ļ�
//    Version        : 1.0
//    Create        : 2004-8-31
//
//////////////////////////////////////////////////////////////////////

#ifndef _com_utility_h
#define _com_utility_h
#include "stdafx.h"
#include <list>
using namespace std;

#define DUMMY_FILE_FILTER       NULL
#define MAX_FILE_PATH_LEN       512
#define ACE_LOG_DIRECTORY ACE_TEXT ("../log/")

namespace SVS_SS_UTILITIES
{
    typedef struct _SVS_TIMESTR_STRUCT
    {
        uint8_t Year[4];
        uint8_t Month[2];
        uint8_t Day[2];
        uint8_t Hour[2];
        uint8_t Minute[2];
        uint8_t Second[2];
    }SVS_TIMESTR_STRUCT, *PSVS_TIMESTR_STRUCT;

    typedef struct _SVS_STR_HH24MISS_STRUCT
    {
        char Hour[2];
        char Min[2];
        char Sec[2];
    }SVS_STR_HH24MISS_STRUCT, *PSVS_STR_HH24MISS_STRUCT;

    /*
        FUNCTION: �Ѹ���λ��ʱ��ת���ɹ�ʱ�׼ʱ��
        DESCRIPTION: ����λ��ʱ��͹�ʱ�׼ʱ�����8Сʱ��
        INPUT:const uint32_t scrTime:�Թ�Ԫ1970��1��1��00:00:00�����������ξ��������
        OUTPUT:�Թ�Ԫ1970��1��1��00:00:00������ʱ�׼ʱ�侭�������
    */
    time_t gwtime2sdtime(const time_t gwTime);
    /*
    * FUNCTION: time_t str2time()
    * DESCRIPTION: ����20040629182030��ʱ�䴮ת��������Ϊ��λ������ʱ��,
    *              ���Թ�ʱ�׼ʱ�乫Ԫ1970��1��1��00:00:00�������������
    * INPUT:
    *     const char *pStr -- ʱ���ַ�
    * OUTPUT:
    *      �Թ�ʱ�׼ʱ�乫Ԫ1970��1��1��00:00:00�������������
    */
    time_t str2time(const char *pStr);

    char *time2manstr(char *pDestBuf, int32_t nbuflen, const time_t t);

    /*
    * FUNCTION: char *time2str()
    * DESCRIPTION: ������ʱ�䣨�Թ�ʱ�׼ʱ�乫Ԫ1970��1��1��00:00:00�������������
    *              ת��Ϊ����"20040630091030"���ַ�
    * INPUT:
    *     char *pDestBuf         -- ����ʱ���ַ�����ָ�루ֵ������
    *     int32_t nbuflen            -- ����ʱ���ַ������С
    *     const time_t *calptr   -- �Թ�ʱ�׼ʱ�乫Ԫ1970��1��1��00:00:00�������������
    * OUTPUT:
    *     ����ָ��pDestBuf
    */
    char *time2str(char *pDestBuf, int32_t nbuflen, const time_t *calptr);

    char *time2str(char *pDestBuf, int32_t nbuflen, const time_t t);
    /*
    * FUNCTION: char *time2str()
    * DESCRIPTION: ������ʱ�䣨�Թ�ʱ�׼ʱ�乫Ԫ1970��1��1��00:00:00�������������
    *              ת��Ϊ����"2004_06_30_09_10_30"���ַ�
    * INPUT:
    *     char *pDestBuf         -- ����ʱ���ַ�����ָ�루ֵ������
    *     int32_t nbuflen            -- ����ʱ���ַ������С
    *     const time_t *calptr   -- �Թ�ʱ�׼ʱ�乫Ԫ1970��1��1��00:00:00�������������
    * OUTPUT:
    *     ����ָ��pDestBuf
    */
    char *time2with_str(char *pDestBuf, int32_t nbuflen, const time_t *calptr);
    char *time2filename(char *pDestBuf, int32_t nbuflen, const time_t t, const char *strSuffix = "vod");
    char *time2bigfilename(char *pDestBuf, int32_t nbuflen, const time_t t, const char *strSuffix = "vod");

    /*
    * FUNCTION: char *GetAppPath()
    * DESCRIPTION: ��Ӧ�ó���ȫ·����ȡӦ�ó���·��
    * INPUT:
    *     const char *pAppName -- Ӧ�ó���ȫ·���ļ���
    *     char *pDestBuf       -- Ӧ�ó���·��
    * OUTPUT:
    *     Ӧ�ó���·��ָ��
    */
    char *GetAppPath(const char *pAppName, char *pAppPaht);

    /*
    * FUNCTION: char *byte2int()
    * DESCRIPTION: ������Ϊlen�ֽڵ��ַ�ת��Ϊһ������
    * INPUT:
    *     const uint8_t *pch --- �ַ�ָ��
    *     int32_t len --- �ַ���
    * OUTPUT:
    *     �ַ��Ӧ������
    */
    int32_t byte2int(const uint8_t *pch, const int32_t len);

    /*
    * FUNCTION: char *timestr2with_timestr()
    * DESCRIPTION: ����ʽΪ20040102030400��ʱ�䴮ת��Ϊ��ʽΪ2004_01_02_03_04_00��ʱ�䴮
    * INPUT:
    *     const uint8_t *pSrc --- Դʱ�䴮
    *     char *pDest --- Ŀ��ʱ�䴮
    * OUTPUT:
    *     ����Ŀ��ʱ�䴮
    */
    char *timestr2with_timestr(const uint8_t *pSrc, char *pDest);
    time_t with_str2time(const uint8_t *pStr, int32_t len);

    /*
    * FUNCTION: time_t hh24miss2second()
    * DESCRIPTION: ����ʽΪ030400(HH24MISS)��ʱ�䴮ת��Ϊ�Ե���00:00:00�������������
    * INPUT:
    *     const char *strTime --- HH24MISS��ʽ��ʱ�䴮
    * OUTPUT:
    *     ����Ŀ��ʱ�䴮
    */
    time_t hh24miss2second(const char *strTime);

    char *bit2str(char *strWeekFlag, const uint8_t by);
    uint8_t str2bit(const char *strWeekFlag);

    /*
    * FUNCTION: bool onlyone_process()
    * DESCRIPTION: �жϽ���Ƿ��Ѿ�����
    * INPUT:
    *     const char *strProgramName --- �����
    * OUTPUT:
    *     ������Ѿ����ڣ���ô����false, ���򷵻�true
    */
    bool onlyon_process(const char *strProgramName);

    /*
    * FUNCTION: relogfilename()
    * DESCRIPTION: �޸���־�ļ����������ݡ�
    * INPUT:
    *     const char * const strFileName  --- ��־�ļ���, ����·��
    * OUTPUT:
    *     �ɹ�����0��ʧ�ܷ���-1.
    */
    int32_t relogfilename(const char * const strFileName);

    /**
     * ����־�ļ�<strFileName>(��·��)���ݣ���Ƹ�Ϊ:
     * strFileNameyyyy_mm_dd_hh24_mi_ss, ����yyyy_mm_dd_hh24_mi_ssΪ��_��_��_ʱ_��_��
     * �ɹ�����0, ʧ�ܷ���-1.
     */
    int32_t backup_log(const char *strFileName);

    /*
    * FUNCTION: get_midnight()
    * DESCRIPTION: ȡ������ҹʱ��,�����ص����Ǳ��ܵĵڼ���
    * INTPUT:
    *   int32_t &weekDay --- ����ֵ�����壬���ص����Ǳ��ܵĵڼ���
    * OUTPUT:
    *   ���ص�����ҹʱ��
    */
    time_t get_midnight(int32_t &weekDay);


    bool only_inetaddr(int32_t nPort);

    //�����0��nRange�������
    int32_t random_range(int32_t nRange);

    //���ļ��Ƶ�ָ����Ŀ¼
    int32_t MoveFile(const char *pszSource, const char *pszDirectory);

    //��ȡָ���ļ����µ������ļ�
    int32_t ScanDir(const char *pszDirectory, std::list<string> &vecFileList,
                int32_t(*pFilterFunc)(const struct dirent *, void *) = DUMMY_FILE_FILTER,
                void *pParamList = NULL);

    //У���ļ����Ƿ���CDR�������
    int32_t IsCdr(const struct dirent *pDirent, void *);

    //�Ƿ�����ȫ��ͬ������Ŀ¼
    int32_t IsDirEqual(const char *pszDir, const char *pszCmpDir);

    //ͨ���ļ������Ƿ�Ϊ���ڵ�CDR�ļ�
    int32_t IsOverdueCdr(const struct dirent *pDirent, void *pParamList);

    int32_t IsPuLog(const struct dirent *pDirent, void *);

    void bit_turn_on(uint16_t& flag, int32_t pos);

    void bit_turn_off(uint16_t& flag, int32_t pos);

    bool bit_on( uint16_t flag, int32_t pos);

    bool bit_off( uint16_t flag, int32_t pos);

    void flip_bit(uint16_t& flag, int32_t pos);
} //namespace SVS_SS_UTILITIES


extern bool is_loopback_ip(const char *strIp);

extern int32_t appexit();

/**
 * ��ȡ�����IP�б�, ����ж��IP��IP֮����'\0'����, ����
 * "192.168.1.256\061.144.207.112", ��ʾ������IP��192.168.1.256��61.144.207.112
 * �ɹ�����IP�ĸ���ʧ�ܷ���-1.
 */
extern int32_t getlocalip(char *strBuf, int32_t nBufSize);

/**
 * ����SOCKET����
 */

extern int32_t open_log_timestamp();
extern int32_t digit_check(const char* str);

extern int32_t CallShellSendEMail(const char *strTitle, const char *strContent);
extern void SetLimitValue( int32_t type, int32_t newValue );
//�������ļ��ж�ȡ��������
/*
;����������Ϣ
[SVS_LOG]
;�������з�ʽ,0:��̨,1:ǰ̨
RunType=0
;��־�������,0:�����,1:���
LogEnable=1
;��־����ȼ�,1:INFO,3:DEBUG,5:WARNING,7:ERROR,9:CRITICAL
LogLM=1
*/
extern int32_t getDebugConfig(const char *svrCfgFile,  int32_t &runType, int32_t &logEnable, int32_t &logLM);

inline void reset_mb_list(ACE_Message_Block *mb)
{
    for(ACE_Message_Block *r = mb; NULL != r; r = r->cont())
        r->reset();
}
/**
 * �������ֽ����ʮ����IP<nIp>ת��Ϊ���ʽIP�ַ��浽����
 * Ϊ<n>�ֽڵĻ�����<buf>��.
 * ���Ƿ���<buf>.
 */
inline const char *decip2str(uint32_t nIp, char *buf, size_t n)
{
    if(NULL == buf)
        return buf;

    ACE_OS::snprintf(buf, n, "%d.%d.%d.%d",
        (nIp & 0xFF000000) >> 24,
        (nIp & 0x00FF0000) >> 16,
        (nIp & 0x0000FF00) >> 8,
        (nIp & 0x000000FF));

    return buf;
}
#define IP10_TO_4BYTES(nIp) \
        ((nIp) & 0xFF000000) >> 24, \
        ((nIp) & 0x00FF0000) >> 16, \
        ((nIp) & 0x0000FF00) >> 8,  \
        ((nIp) & 0x000000FF)

#ifdef WIN32
struct TCP_KEEPALIVE
{
    u_long onoff;
    u_long keepalivetime;
    u_long keepaliveinterval;
};

#define SIO_KEEPALIVE_VALS  _WSAIOW(IOC_VENDOR, 4)

#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH




#endif //WIN32

#define CHECK_OK     0
#define CHECK_ERROR -1

#define LENSDEVID_LEN_C05 18

//����豸ID��Ч��
int32_t CheckDeviceID(const char *pszDevID);

// ����豸id��������ͷ��id�ĳ������ƣ�ĿǰRTɾ���ú���
int32_t CheckDeviceIDEx(const char * pszDevID);

enum ENUM_SVS_DELETE_MULTI
{
    SVS_DELETE_SINGLE = 0,
    SVS_DELETE_MULTI  = 1,
};


template<class T>
T* SVS_NEW(T* &m, uint32_t nMuili = 0)
{
    try
    {
        if (nMuili == 0)
        {
            m = new(T);
        }
        else
        {
            m = new T[nMuili] ;
        }
        return m;
    }
    catch(...)
    {
        m = NULL;
    }
    return NULL;
};

template<class T>
void SVS_DELETE(T* &m, uint32_t nMuili = SVS_DELETE_SINGLE)
{
    if(NULL == m)
    {
        return;
    }


    if (SVS_DELETE_SINGLE == nMuili)
    {
        delete m;
    }
    else
    {
        delete[] m;
    }


    m = NULL;
};


#endif //_com_utility_h


