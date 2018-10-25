/**
* @file    svs_adapter_vms_loger.h
* @brief   SvsЭ��ջ��־�����
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-10
*/

#include "svs_adapter_vms_loger.h"
#include "svs_log_msg.h"

CSvsLoger::CSvsLoger()
{
}

CSvsLoger::~CSvsLoger()
{
}

void CSvsLoger::writeLog(uint8_t ucType,
                         uint8_t ucLevel,
                         const char *pszLog,
                         uint32_t unLogLength,
                         char* pFile,
                         int32_t lLine)
{
   int32_t __ace_error = ACE_Log_Msg::last_error_adapter ();
   ACE_Log_Msg *ace___ = ACE_Log_Msg::instance ();
   ace___->conditional_set (pFile, lLine, 0, __ace_error);
   ace___->file( pFile );
   ace___->linenum( lLine );
   (void) SVS_Log_Msg::log(ucLevel,pszLog);
   ACE_OS::last_error( 0 );
   return;
}

