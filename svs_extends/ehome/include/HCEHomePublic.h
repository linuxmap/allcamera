#ifndef _HC_EHOME_PUBLIC_H_
#define _HC_EHOME_PUBLIC_H_ 

//��SDK��������
#ifndef _HC_NET_SDK_H_

/*******************ƽ̨��ص��������Ͷ��� begin**********************/    

#if (defined(_WIN32)) //windows
    #define NET_DVR_API  extern "C"__declspec(dllimport)
    typedef  unsigned __int64 UINT64;
#elif defined(__linux__) || defined(__APPLE__) //linux
    typedef    unsigned int    DWORD;
    typedef    unsigned short  WORD;
    typedef    unsigned short  USHORT;
    typedef    short           SHORT;
    typedef    int             LONG;
    typedef    unsigned char     BYTE;
    #define    BOOL            int
    typedef    unsigned int    UINT;
    typedef    void*           LPVOID;
    typedef    void*           HANDLE;
    typedef    unsigned int*   LPDWORD; 
    typedef    unsigned long long UINT64;

    #ifndef    TRUE
    #define    TRUE    1
    #endif
    #ifndef    FALSE
    #define       FALSE 0
    #endif
    #ifndef    NULL
    #define       NULL 0
    #endif

    #define __stdcall 
    #define CALLBACK  

    #define NET_DVR_API extern "C"
#endif //linux

/*******************ƽ̨��ص��������Ͷ��� end**********************/    

/*******************ȫ�ִ����� begin**********************/
#define NET_DVR_NOERROR                 0   //û�д���
#define NET_DVR_PASSWORD_ERROR          1   //�û����������
#define NET_DVR_NOENOUGHPRI             2   //Ȩ�޲���
#define NET_DVR_NOINIT                  3   //û�г�ʼ��
#define NET_DVR_CHANNEL_ERROR           4   //ͨ���Ŵ���
#define NET_DVR_OVER_MAXLINK            5   //���ӵ�DVR�Ŀͻ��˸����������
#define NET_DVR_VERSIONNOMATCH          6   //�汾��ƥ��
#define NET_DVR_NETWORK_FAIL_CONNECT    7   //���ӷ�����ʧ��
#define NET_DVR_NETWORK_SEND_ERROR      8    //�����������ʧ��
#define NET_DVR_NETWORK_RECV_ERROR      9    //�ӷ�������������ʧ��
#define NET_DVR_NETWORK_RECV_TIMEOUT    10    //�ӷ������������ݳ�ʱ
#define NET_DVR_NETWORK_ERRORDATA       11    //���͵���������
#define NET_DVR_ORDER_ERROR             12    //���ô������
#define NET_DVR_OPERNOPERMIT            13    //�޴�Ȩ��
#define NET_DVR_COMMANDTIMEOUT          14    //DVR����ִ�г�ʱc

#define NET_DVR_PARAMETER_ERROR         17  //��������

#define NET_DVR_NOSUPPORT               23    //��������֧��

#define    NET_DVR_DVROPRATEFAILED         29  //DVR����ʧ��

#define NET_DVR_DIR_ERROR               40    //·������
#define NET_DVR_ALLOC_RESOURCE_ERROR    41  //��Դ�������
#define NET_DVR_AUDIO_MODE_ERROR        42    //����ģʽ����
#define NET_DVR_NOENOUGH_BUF            43    //������̫С
#define NET_DVR_CREATESOCKET_ERROR      44    //����SOCKET����
#define NET_DVR_SETSOCKET_ERROR         45    //����SOCKET����
#define NET_DVR_MAX_NUM                 46  //�����ﵽ���
#define NET_DVR_USERNOTEXIST            47    //�û�������

#define NET_DVR_GETLOCALIPANDMACFAIL    53  //��ñ��ص�IP��ַ�������ַʧ��

#define NET_DVR_VOICEMONOPOLIZE         69    //��������ռ

#define NET_DVR_CREATEDIR_ERROR         71    //������־�ļ�Ŀ¼ʧ��
#define NET_DVR_BINDSOCKET_ERROR        72    //���׽���ʧ��
#define NET_DVR_SOCKETCLOSE_ERROR       73    //socket�����жϣ��˴���ͨ�������������жϻ�Ŀ�ĵز��ɴ�
#define NET_DVR_USERID_ISUSING          74    //ע��ʱ�û�ID���ڽ���ĳ����
#define NET_DVR_SOCKETLISTEN_ERROR      75    //����ʧ��

#define NET_DVR_CONVERT_SDK_ERROR       85    //����ת���ʧ��

#define NET_DVR_FUNCTION_NOT_SUPPORT_OS 98  //�˹��ܲ�֧�ָò���ϵͳ

#define NET_DVR_USE_LOG_SWITCH_FILE     103 //����ʹ����־�����ļ�

#define    NET_DVR_PACKET_TYPE_NOT_SUPPORT 105    //������װ��ʽ����



//�����Խ��������
#define  NET_AUDIOINTERCOM_OK                   600 //�޴���
#define  NET_AUDIOINTECOM_ERR_NOTSUPORT         601 //��֧��
#define  NET_AUDIOINTECOM_ERR_ALLOC_MEMERY      602 //�ڴ��������
#define  NET_AUDIOINTECOM_ERR_PARAMETER         603 //��������
#define  NET_AUDIOINTECOM_ERR_CALL_ORDER        604 //���ô������
#define  NET_AUDIOINTECOM_ERR_FIND_DEVICE       605 //δ�����豸
#define  NET_AUDIOINTECOM_ERR_OPEN_DEVICE       606 //���ܴ��豸��
#define  NET_AUDIOINTECOM_ERR_NO_CONTEXT        607 //�豸�����ĳ���
#define  NET_AUDIOINTECOM_ERR_NO_WAVFILE        608 //WAV�ļ�����
#define  NET_AUDIOINTECOM_ERR_INVALID_TYPE      609 //��Ч��WAV��������
#define  NET_AUDIOINTECOM_ERR_ENCODE_FAIL       610 //����ʧ��
#define  NET_AUDIOINTECOM_ERR_DECODE_FAIL       611 //����ʧ��
#define  NET_AUDIOINTECOM_ERR_NO_PLAYBACK       612 //����ʧ��
#define  NET_AUDIOINTECOM_ERR_DENOISE_FAIL      613 //����ʧ��
#define  NET_AUDIOINTECOM_ERR_UNKOWN            619 //δ֪����

/*******************ȫ�ִ����� begin**********************/    
#define MAX_PASSWD_LEN      32
#define NAME_LEN            32      //�û�������

#endif //_HC_NET_SDK_H_
#define MAX_DEVICE_ID_LEN    256     //�豸ID����
#define NET_EHOME_SERIAL_LEN 12


typedef struct tagNET_EHOME_IPADDRESS
{
    char szIP[128]; 
    WORD wPort;     //�˿�
    char byRes[2];
}NET_EHOME_IPADDRESS, *LPNET_EHOME_IPADDRESS;

typedef struct tagNET_EHOME_ZONE
{
    DWORD dwX;          //X������
    DWORD dwY;          //Y������
    DWORD dwWidth;  //���
    DWORD dwHeight;    //�߶�
}NET_EHOME_ZONE, *LPNET_EHOME_ZONE;

//��������
typedef enum tagNET_EHOME_LOCAL_CFG_TYPE
{
    UNDEFINE = -1,   //��ʱû�о���Ķ���
    ACTIVE_ACCESS_SECURITY = 0, //�豸��������İ�ȫ��
    AMS_ADDRESS = 1,            //�������������ػػ���ַ
    SEND_PARAM = 2,                 //���Ͳ�������
}NET_EHOME_LOCAL_CFG_TYPE, *LPNET_EHOME_LOCAL_CFG_TYPE;

typedef struct tagNET_EHOME_LOCAL_ACCESS_SECURITY
{
    DWORD   dwSize;
    BYTE    byAccessSecurity;    //0-����ģʽ����������汾��Э����룩��1-��ͨģʽ��ֻ֧��4.0���°汾����֧��Э�鰲ȫ�İ汾���룩 2-��ȫģʽ��ֻ����4.0���ϰ汾��֧��Э�鰲ȫ�İ汾���룩
    BYTE    byRes[127];
}NET_EHOME_LOCAL_ACCESS_SECURITY,*LPNET_EHOME_LOCAL_ACCESS_SECURITY;

typedef struct tagNET_EHOME_AMS_ADDRESS
{
    DWORD dwSize;
    BYTE  byEnable;  //0-�ر�CMS���ձ������ܣ�1-����CMS���ձ�������
    BYTE  byRes1[3];
    NET_EHOME_IPADDRESS  struAddress;    //AMS���ػػ���ַ
    BYTE byRes2[32];
}NET_EHOME_AMS_ADDRESS, *LPNET_EHOME_AMS_ADDRESS;

typedef struct tagNET_EHOME_SEND_PARAM
{
    DWORD dwSize;
    DWORD dwRecvTimeOut;    //���ճ�ʱʱ�䣬��λ����
    BYTE  bySendTimes;      //���ķ��ʹ�����Ϊ��Ӧ�����绷���ϲ������£������������Ĭ��һ�Σ����3��
    BYTE  byRes2[127];
}NET_EHOME_SEND_PARAM, *LPNET_EHOME_SEND_PARAM;


#endif //_HC_EHOME_PUBLIC_H_