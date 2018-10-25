
#ifndef _SVS_STACK_COMM_H_
#define _SVS_STACK_COMM_H_

#include <stdio.h>
#include <stdarg.h>
#include "svs_vms_stack.h"
#include <vms/vms.h>

#pragma pack(push, 1)


enum LINK_TYPE
{
    LINK_TYPE_SERVER = 0x1,
    LINK_TYPE_CLIENT = 0x2,
    LINK_TYPE_INNER  = 0x3
};


enum CONN_FLAG
{
    CONN_FLAG_IDLE,
    CONN_FLAG_CONNECTING,
    CONN_FLAG_CONNECTED,
    CONN_FLAG_CONNTIMEOUT,
    CONN_FLAG_CLOSING,
    CONN_FLAG_CLOSED
};

typedef struct _SENDMSGHEAD
{
    uint32_t    unSendSize;
    uint32_t    unTimeout;
    time_t      tSendTime;
}SENDMSGHEAD;

#define INVALID_SOCK_FD         -1
#define INVALID_IP              INADDR_ANY
#define INVALID_PORT            0               // �Ƿ��˿�
#define MAX_EPOLL_FD             1024
#define MAX_RECV_BUFF            (512*1024)    // Э��ջ���ջ������С
#define MAX_SOCKET_BUFFSIZE     (512*1024)    // socket�������С
#define EPOLL_WAIT_EVENT_TIME   10           // epoll��Ӧ���ȴ��¼��ĳ�ʱʱ�䣬��λ����
#define THREAD_STACK_SIZE       (128*1024)   // �̶߳�ջ��С
#define MAX_QUEUE_SIZE           2000          // ��Ϣ�����������
#define STR_IP_LEN               16           // ��ֽṹ��ip��ַ������'\0'
#define DATE_TIME_LEN            15          // ����ʱ�䣬����'\0'

#define MAX_LOG_LENTH               1024                         // ÿ����־����־����������ֵ
#define SVS_MSG_HEARTBEAT_SIZE     sizeof(SVS_MSG_HEARTBEAT)   // SVS������Ϣ�ĳ���
#define SENDMSGHEAD_SIZE            sizeof(SENDMSGHEAD)

#define HEARTBEAT_THREAD_WORK_INTERVAL    1         // �������̼߳�����ڣ���λ��
#define HEARTBEAT_TIMEOUT_TIMES           3         // �������ʱ����

#define REQ_MSG_TRANSNO_MASK  0x7FFF
#define RESP_MSG_TRANSNO_MASK 0x8000

#pragma pack(pop)

extern ISvsLogger* g_pLogger;
static void printSvsLog(uint8_t ucType, uint8_t ucLevel, char * pFile, int32_t lLine, const char *pFormat, ...)
{
    if (NULL == g_pLogger)
    {
        return;
    }

    char szBuff[MAX_LOG_LENTH + 1] = {0};
    va_list args;/*lint !e530*/// args����Ϊnull��64λ��������������
    va_start(args, pFormat);
    int32_t nLen = vsnprintf(szBuff, MAX_LOG_LENTH, pFormat, args);/*lint !e530*/// args����Ϊnull��64λ��������������
    szBuff[nLen] = '\0';
    g_pLogger->writeLog(ucType, ucLevel, szBuff, (uint32_t)nLen, pFile, lLine);
    va_end(args);
    return;
}

#define VMS_SVS_LOG(ucType, ucLevel, arg...) \
    printSvsLog(ucType, ucLevel, (char*)__FILE__, (int32_t)__LINE__, ##arg);


#endif

