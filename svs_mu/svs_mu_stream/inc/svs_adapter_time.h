/*
 * File:   svs_adapter_time.h
 * Author:
 *
 * Created on 2010��12��8��, ����3:48
 */

#ifndef __SVSTIME_H__
#define __SVSTIME_H__

#include <vms/vms.h>
// ��ȡϵͳ�뼶ʱ��
uint32_t SVS_GetSecondTime();

namespace SVS_TIME
{
    time_t str2time(const char *pStr);

    char *time2str(char *pDestBuf, int32_t nbuflen, const time_t t);

   char *time2bigfilename(char *pDestBuf, int32_t nbuflen, const time_t t, const char *strSuffix = "vod");

}//namespace SVS_TIME


#endif    /* __SVSTIME_H__ */

