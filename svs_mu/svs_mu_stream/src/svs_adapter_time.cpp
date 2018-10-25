/*
 * File:   SvsTime.cpp
 * Author:
 *
 * Created on 2010年12月8日, 下午3:50
 */
#include <sys/time.h>
#include "svs_ace_header.h"
#include "svs_adapter_time.h"

uint32_t SVS_GetSecondTime()
{
    struct timeval now;
    (void) gettimeofday(&now, 0);

    return (uint32_t)now.tv_sec;
}

//形如20040629182030的时间串转换成以秒为单位的日历时间,
//即自国际标准时间公元1970年1月1日00:00:00以来经过的秒数。
time_t SVS_TIME::str2time(const char *pStr)
{
    struct tm tmvalue;

    (void)ACE_OS::memset(&tmvalue, 0, sizeof(tmvalue));

    const char *pch = pStr;
    char tmpstr[8];
    memcpy(tmpstr, pch, 4);
    tmpstr[4] = '\0';
    tmvalue.tm_year = atoi(tmpstr) - 1900;
    pch += 4;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_mon = atoi(tmpstr) - 1;
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_mday = atoi(tmpstr);
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_hour = atoi(tmpstr);
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_min = atoi(tmpstr);
    pch += 2;

    memcpy(tmpstr, pch, 2);
    tmpstr[2] = '\0';
    tmvalue.tm_sec = atoi(tmpstr);

    return ACE_OS::mktime(&tmvalue);
}

char *SVS_TIME::time2str(char *pDestBuf, int32_t nbuflen, const time_t t)
{
    struct tm tmv;

    (void)ACE_OS::localtime_r( &t, &tmv );

    (void)ACE_OS::snprintf(pDestBuf,
                     (size_t)nbuflen,
                     "%04d%02d%02d%02d%02d%02d",
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     tmv.tm_min,
                     tmv.tm_sec);

    return pDestBuf;
}

char *SVS_TIME::time2bigfilename(char *       pDestBuf,
                                  int32_t          nbuflen,
                                  const time_t t,
                                  const char * strSuffix)
{
    struct tm tmv;

    (void)ACE_OS::localtime_r( &t, &tmv );
    (void)ACE_OS::snprintf(pDestBuf,
                     (size_t)nbuflen,
                     "%04d%02d%02d%02d%s",
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     strSuffix);

    return pDestBuf;
}



