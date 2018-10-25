#ifndef _SVS_Print_Version_h
#define _SVS_Print_Version_h

//������������Ϣ
#define SVR_VERSION_INFO       "svs v1.0.0.1"
#define SVR_UPDATE_DATE         __DATE__
#define SVR_UPDATE_TIME         __TIME__

#include <stdio.h>
#include <ace/Get_Opt.h>
#include "svs_log_msg.h"


/*****************************************************************************
 �� �� ��  : print_server_info
 ��������  : ��ӡ�������汾��Ϣ
 �������  : int32_t argc                            // main�������
             ACE_TCHAR *argv[]                   // main����
             const char * const strServerName    // ������������������
 �������  : ��
 �� �� ֵ  : void
 ���ú���  :
 ��������  :
*****************************************************************************/
inline void print_server_info(int32_t argc, ACE_TCHAR *argv[], const char * const strServerName)
{
        ACE_Get_Opt getOption(argc, argv, ACE_TEXT("vV"), 1);

        for (int32_t c; (c = getOption ()) != -1; )
        switch (c)
        {
        case 'v':
        case 'V':
                printf("%s Version: %s, Update: %s %s \n",
                       strServerName,
                       SVR_VERSION_INFO,
                       SVR_UPDATE_DATE,
                       SVR_UPDATE_TIME);
                exit(0);
        default:
                printf("%s Version: %s Update: %s %s \n",
                       strServerName,
                       SVR_VERSION_INFO,
                       SVR_UPDATE_DATE,
                       SVR_UPDATE_TIME);
                printf("Usage:\n \t %s -v\n", argv[0]);
                exit(0);
        }

        SVS_LOG((SVS_LM_INFO,
                "\nVersion Info:\nDesc: %s, Version: %s, Update: %s %s\n",
                strServerName,
                SVR_VERSION_INFO,
                SVR_UPDATE_DATE,
                SVR_UPDATE_TIME));

        return;
}


#endif //_SVS_Print_Version_h



