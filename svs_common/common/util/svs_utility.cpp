#include "svs_ace_header.h"
#include "svs_utility.h"
#include "stdafx.h"
#include "svs_config.h"
#include "svs_log_msg.h"
#include <stdlib.h>
#include "vms/vms.h"


time_t SVS_SS_UTILITIES::gwtime2sdtime(const time_t gwTime)
{
    struct tm tmv;
    char buff[15] = {0,};
    (void)ACE_OS::gmtime_r(&gwTime,&tmv);
    (void)strftime(buff,sizeof(buff),"%Y%m%d%H%M%S",&tmv);
    return str2time(buff);
}

time_t SVS_SS_UTILITIES::str2time(const char *pStr)
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

char* SVS_SS_UTILITIES::time2manstr(char *pDestBuf, int32_t nbuflen, const time_t t)
{
    struct tm tmv;
    (void)ACE_OS::localtime_r(&t, &tmv );

    (void)ACE_OS::snprintf(   pDestBuf,
                        (size_t)nbuflen,
                        "%04d-%02d-%02d %02d:%02d:%02d",
                        tmv.tm_year + 1900,
                        tmv.tm_mon + 1,
                        tmv.tm_mday,
                        tmv.tm_hour,
                        tmv.tm_min,
                        tmv.tm_sec);

    return pDestBuf;
}

char *SVS_SS_UTILITIES::time2str(char *pDestBuf, int32_t nbuflen, const time_t *calptr)
{
    struct tm tmv;

    (void)ACE_OS::localtime_r( calptr, &tmv );

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

char *SVS_SS_UTILITIES::time2str(char *pDestBuf, int32_t nbuflen, const time_t t)
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

char *SVS_SS_UTILITIES::time2with_str(char *pDestBuf, int32_t nbuflen, const time_t *calptr)
{
    struct tm tmv;

    (void)ACE_OS::localtime_r( calptr, &tmv );

    (void)ACE_OS::snprintf(   pDestBuf,
                        (size_t)nbuflen,
                        "%04d_%02d_%02d_%02d_%02d_%02d",
                        tmv.tm_year + 1900,
                        tmv.tm_mon + 1,
                        tmv.tm_mday,
                        tmv.tm_hour,
                        tmv.tm_min,
                        tmv.tm_sec);

    return pDestBuf;
}

char *SVS_SS_UTILITIES::time2filename(char *       pDestBuf,
                                      int32_t          nbuflen,
                                      const time_t t,
                                      const char * strSuffix)
{
    struct tm tmv;

    (void)ACE_OS::localtime_r( &t, &tmv );
    (void)ACE_OS::snprintf(pDestBuf,
                     (size_t)nbuflen,
                     "%04d_%02d_%02d_%02d_%02d_%02d.%s",
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     tmv.tm_min,
                     tmv.tm_sec,
                     strSuffix);

    return pDestBuf;
}

char *SVS_SS_UTILITIES::time2bigfilename(char *       pDestBuf,
                                      int32_t          nbuflen,
                                      const time_t t,
                                      const char * strSuffix)
{
    struct tm tmv;

    (void)ACE_OS::localtime_r( &t, &tmv );
    (void)ACE_OS::snprintf(pDestBuf,
                     (size_t)nbuflen,
                     "%04d%02d%02d%02d.%s",
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     strSuffix);

    return pDestBuf;
}

char *SVS_SS_UTILITIES::GetAppPath(const char *pAppName, char *pAppPath)
{
    (void)ACE_OS::strcpy(pAppPath, pAppName);
    char *pch = pAppPath + strlen(pAppPath) - 1;
    while (pch >= pAppPath)
    {
        if ('\\' == *pch)
        {
            *pch = '\0';
            return pAppPath;
        }
        else
        {
            pch--;
        }
    }

    (void)ACE_OS::strcpy(pAppPath, "./");
    return pAppPath;
}

int32_t SVS_SS_UTILITIES::byte2int(const uint8_t *pch, const int32_t len)
{
    char buf[128];

    (void)ACE_OS::memcpy(buf, pch, (size_t)len);
    buf[len] = '\0';
    return ACE_OS::atoi(buf);
}

char *SVS_SS_UTILITIES::timestr2with_timestr(const uint8_t *pSrc, char *pDest)
{
    PSVS_TIMESTR_STRUCT pTimeStrStruct = (PSVS_TIMESTR_STRUCT)(void *)pSrc;

    (void)ACE_OS::memcpy(pDest, pTimeStrStruct->Year, sizeof(pTimeStrStruct->Year));
    pDest   += sizeof(pTimeStrStruct->Year);
    *pDest++ = '_';

    (void)ACE_OS::memcpy(pDest, pTimeStrStruct->Month, sizeof(pTimeStrStruct->Month));
    pDest   += sizeof(pTimeStrStruct->Month);
    *pDest++ = '-';

    (void)ACE_OS::memcpy(pDest, pTimeStrStruct->Day, sizeof(pTimeStrStruct->Day));
    pDest   += sizeof(pTimeStrStruct->Day);
    *pDest++ = '_';

    (void)ACE_OS::memcpy(pDest, pTimeStrStruct->Hour, sizeof(pTimeStrStruct->Hour));
    pDest   += sizeof(pTimeStrStruct->Hour);
    *pDest++ = '_';

    (void)ACE_OS::memcpy(pDest, pTimeStrStruct->Minute, sizeof(pTimeStrStruct->Minute));
    pDest   += sizeof(pTimeStrStruct->Minute);
    *pDest++ = '_';

    (void)ACE_OS::memcpy(pDest, pTimeStrStruct->Second, sizeof(pTimeStrStruct->Second));
    pDest   += sizeof(pTimeStrStruct->Second);
    *pDest++ = '\0';

    return pDest;
}

time_t SVS_SS_UTILITIES::with_str2time(const uint8_t *pStr, int32_t len)
{
#pragma pack(push,1)
    struct with_timestr
    {
        char year[5];
        char month[3];
        char day[3];
        char hour[3];
        char min[3];
        char sec[3];
    };
#pragma pack(pop)

    char temp[512];

    (void)ACE_OS::memcpy(temp, pStr, (size_t)len);
    temp[len] = '\0';
    struct with_timestr *ptr = (struct with_timestr *)temp;

    struct tm tmvalue;
    (void)ACE_OS::memset(&tmvalue, 0, sizeof(tmvalue));
    ptr->year[4] = '\0';
    tmvalue.tm_year = ACE_OS::atoi(ptr->year) - 1900;

    ptr->month[2]  = '\0';
    tmvalue.tm_mon = ACE_OS::atoi(ptr->month) - 1;

    ptr->day[2] = '\0';
    tmvalue.tm_mday = ACE_OS::atoi(ptr->day);

    ptr->hour[2] = 0;
    tmvalue.tm_hour = ACE_OS::atoi(ptr->hour);

    ptr->min[2] = 0;
    tmvalue.tm_min = ACE_OS::atoi(ptr->min);

    ptr->sec[2] = 0;
    tmvalue.tm_sec = ACE_OS::atoi(ptr->sec);

    return ACE_OS::mktime(&tmvalue);
}

//����ʽΪ030400(HH24MISS)��ʱ�䴮ת��Ϊ�Ե���00:00:00��������������
time_t SVS_SS_UTILITIES::hh24miss2second(const char *strTime)
{
    if (NULL == strTime)
    {
        SVS_LOG((SVS_LM_ERROR, "convert time value to strTime error. strTime is NULL."));
        return 0;
    }

    const int32_t hh24missLen = 6;  //ʱ�䴮�ĳ���
    char buf[24];
    (void)ACE_OS::memcpy(buf, strTime, hh24missLen);
    buf[hh24missLen] = '\0';
    time_t ret = atoi(((PSVS_STR_HH24MISS_STRUCT)buf)->Sec);  //seconds


    //    assert(ret >= 0 && ret < 60);
    if ((0 > ret) || (60 <= ret))
    {
        SVS_LOG((SVS_LM_ERROR, "convert time value to strTime error. value sec[%d] is error.", ret));

        return 0;
    }

    {
        ((PSVS_STR_HH24MISS_STRUCT)buf)->Sec[0] = '\0';
        time_t tmMinute = atoi(((PSVS_STR_HH24MISS_STRUCT)buf)->Min);    //����


//        assert(t >= 0 && t < 60);
        if ((0 > tmMinute) || (60 <= tmMinute))
        {
            SVS_LOG((SVS_LM_ERROR, "convert time value to strTime error. value min[%d] is error.", tmMinute));

            return 0;
        }

        ret += (tmMinute * 60);

        ((PSVS_STR_HH24MISS_STRUCT)buf)->Min[0] = '\0';
        time_t tmHour = atoi(((PSVS_STR_HH24MISS_STRUCT)buf)->Hour);  //Сʱ


//        assert(t >= 0 && t <= 24);
        if ((0 > tmHour) || (24 < tmHour))
        {
            SVS_LOG((SVS_LM_ERROR, "convert time value to strTime error. value hour[%d] is error.", tmHour));

            return 0;
        }


        ret += (tmHour * 60 * 60);
    }
    return ret;
}

char *SVS_SS_UTILITIES::bit2str(char *strWeekFlag, const uint8_t by)
{
    uint8_t ch = (uint8_t)by;

    int32_t n = 0;

    for (n = 0; n < 8; n++)
    {
        strWeekFlag[n] = (0 == (ch & 0x01)) ? '0' : '1';
        ch >>= 1;
    }

    strWeekFlag[n] = '\0';

    return strWeekFlag;
}

uint8_t SVS_SS_UTILITIES::str2bit(const char *strWeekFlag)
{
    uint8_t ch = 0x00;

    for (int32_t i = 0; i < 8; i++)
    {
        if ('1' == strWeekFlag[i])
        {
            ch |= (uint8_t)(0x01 << i);
        }
    }

    return ch;
}

bool SVS_SS_UTILITIES::onlyon_process(const char *strProgramName)
{
#ifndef WIN32
    sem_t *sem = sem_open(strProgramName,
                          O_RDWR | O_CREAT | O_EXCL,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1);
    if (sem == SEM_FAILED)
    {
        if (EEXIST == errno)
        {}
        else
        {}

        return false;
    }
#endif //WIN32


    return true;
}

int32_t SVS_SS_UTILITIES::relogfilename(const char * const strFileName)
{
    char strOldFileName[512];

    (void)ACE_OS::snprintf(strOldFileName,
                     sizeof(strOldFileName),
                     "%s%s.log",
                     ACE_LOG_DIRECTORY,
                     strFileName);

    char strNewFileName[512];
    time_t t = time(NULL);

    struct tm tmv;


    //    ACE_OS::memset(&tmv, 0, sizeof(tmv));
    //    ACE_OS::memcpy(&tmv, ACE_OS::localtime(&t), sizeof(tmv));
    (void)ACE_OS::localtime_r( &t, &tmv );

    (void)ACE_OS::snprintf(strNewFileName,
                     sizeof(strNewFileName),
                     "%s%04d_%02d_%02d_%02d_%02d_%02d",
                     strOldFileName,
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     tmv.tm_min,
                     tmv.tm_sec);

    (void)ACE_OS::rename(strOldFileName, strNewFileName);

    return 0;
}

int32_t SVS_SS_UTILITIES::backup_log(const char *strOldFileName)
{
    char strNewFileName[512];
    time_t t = time(NULL);
    struct tm tmv;


    //    ACE_OS::memset(&tmv, 0, sizeof(tmv));
    //    ACE_OS::memcpy(&tmv, ACE_OS::localtime(&t), sizeof(tmv));
    (void)ACE_OS::localtime_r( &t, &tmv );

    (void)ACE_OS::snprintf(strNewFileName,
                     sizeof(strNewFileName),
                     "%s%04d_%02d_%02d_%02d_%02d_%02d",
                     strOldFileName,
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     tmv.tm_min,
                     tmv.tm_sec);

    (void)ACE_OS::rename(strOldFileName, strNewFileName);

    return 0;
}

time_t SVS_SS_UTILITIES::get_midnight(int32_t &weekDay)
{
    time_t curTime = time(NULL);  //��ǰʱ��

    struct tm tmv;


    //    memset(&tmv, 0, sizeof(tmv));
    //    memcpy(&tmv, ACE_OS::localtime(&curTime), sizeof(tmv));
    (void)ACE_OS::localtime_r( &curTime, &tmv );
    weekDay = tmv.tm_wday; //0 ~ 6

    //��ҹʱ��
    tmv.tm_hour = 0;
    tmv.tm_min = 0;
    tmv.tm_sec = 0;

    return mktime(&tmv);
}

bool SVS_SS_UTILITIES::only_inetaddr(int32_t nPort)
{
    ACE_HANDLE handle = ACE_OS::socket(PF_INET, SOCK_STREAM, 0);

    if (ACE_INVALID_HANDLE == handle)
    {
        SVS_LOG((SVS_LM_ERROR, "socket error."));
        return false;
    }

    struct sockaddr_in addr;
    (void)ACE_OS::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)nPort);

    if (-1 == ACE_OS::bind(handle, (struct sockaddr *)(void *)&addr, sizeof(addr)))
    {
        SVS_LOG((SVS_LM_ERROR, "A instance is running."));
        (void)ACE_OS::closesocket(handle);
        return false;
    }

    (void)ACE_OS::closesocket(handle);
    return true;
}

int32_t SVS_SS_UTILITIES::random_range(int32_t nRange)
{
    srand((uint32_t)time(NULL));

    return (int32_t)((((double)rand()) / ((double)RAND_MAX + 1.0)) * (double)nRange);
}

int32_t SVS_SS_UTILITIES::MoveFile(const char *pszSource, const char *pszDirectory)
{
    //���Դ�ļ��Ƿ���ڲ��ҿ�д
    int32_t iRet = access(pszSource, W_OK);
    if (-1 == iRet)
    {
        SVS_LOG((SVS_LM_WARNING, "Source file[%s] cannot access.", pszSource));
        return -1;
    }

    //�ж��ļ��Ƿ�Ϊ��ͨ�ļ�������Ŀ¼
    struct stat stStat;
    iRet = stat(pszSource, &stStat);
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING, "Source file[%s] cannot stat.", pszSource));
        return -1;
    }
    //���Ҫ������Դ�ļ�������ͨ�ļ������ܽ��п���
    if (!S_ISREG(stStat.st_mode))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Source file[%s] is not a regular file.", pszSource));
        return -1;
    }
    //����ļ�Ŀ¼���ܷ��ʣ��򿽱�ʧ��
    iRet = access(pszDirectory, W_OK);
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING, "Directory[%s] cannot access.", pszDirectory));
        return -1;
    }
    char szNewFilePath[512] = {0};
    //�Ը�Ŀ¼��������
    if (0 != strcmp(pszDirectory, "/"))
    {
        size_t uiPathLen = strlen(pszDirectory);
        for (int32_t i = uiPathLen - 1; i > 0; i--)
        {
            if (pszDirectory[i] == '/')
                uiPathLen--;
            else
                break;
        }
        strncpy(szNewFilePath, pszDirectory, uiPathLen + 1);
        strcat(szNewFilePath, "/");
    }
    else
    {
        strcpy(szNewFilePath, pszDirectory);
    }
    //��ȡ�ļ������Ŀ���ļ�·��
    char *pszBasename = basename((char*)pszSource);
    strcat(szNewFilePath, pszBasename);
    //���жϿ�����·���Ƿ����
    iRet = access(szNewFilePath, F_OK);
    if (0 == iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "File[%s] already exists in directory[%s].",
            pszBasename, pszDirectory));
        return -1;
    }

    SVS_LOG((SVS_LM_INFO, "Move file from %s to %s.", pszSource, szNewFilePath));

    //�ȳ���ͨ�����������ƶ����ļ�
    iRet = rename(pszSource, szNewFilePath);
    if (0 != iRet)
    {
        //�������ͬһ�������ϣ���Ҫ�����ļ�����
        if (errno == EXDEV)
        {
            int32_t iFromFd, iToFd, iReadLen;
            char    szBuf[512] = {0};
            if ((iFromFd = open(pszSource, O_RDONLY)) == -1)
            {
                SVS_LOG((SVS_LM_WARNING, "Open source file[%s] error.", pszSource));
                return -1;
            }
            if ((iToFd = open(szNewFilePath, O_WRONLY|O_CREAT|O_TRUNC)) == -1)
            {
                SVS_LOG((SVS_LM_WARNING, "Open dest file[%s] error.", szNewFilePath));
                return -1;
            }

            while ((iReadLen = read(iFromFd, szBuf, 512)) > 0)
            {
                //��д�ļ�ʧ�ܵ�ʱ��ɾ���Ѿ������Ĳ���
                if (write(iToFd, szBuf, iReadLen) != iReadLen
                    || (iReadLen == -1))
                {
                    SVS_LOG((SVS_LM_WARNING, "Write dest file[%s] failed.", szNewFilePath));
                    (void)close(iFromFd);
                    (void)close(iToFd);
                    (void)unlink(szNewFilePath);
                    return -1;
                }
            }
            (void)close(iFromFd);
            (void)close(iToFd);
            (void)unlink(pszSource);
            return 0;
        }

        SVS_LOG((SVS_LM_WARNING, "Move file[%s] to [%s] error.", pszSource, szNewFilePath));
        return -1;
    }

    return 0;
}

int32_t SVS_SS_UTILITIES::IsCdr(const struct dirent *pDirent, void *)
{
    if (NULL == pDirent)
    {
        return -1;
    }

    if (pDirent->d_type == DT_REG)
    {
        uint32_t uiCmsId, uiIndex;
        char szDate[9] = {0};
        char szTime[7] = {0};
        int32_t iRet = sscanf(pDirent->d_name, "%u_%08s_%06s_%03d",
                &uiCmsId, szDate, szTime, &uiIndex);
        if (4 == iRet)
        {
            char szCdrFile[MAX_FILE_PATH_LEN] = {0};
            (void)snprintf(szCdrFile, sizeof(szCdrFile),
                "%u_%s_%s_%.3d", uiCmsId, szDate, szTime, uiIndex);
            if (0 == strcmp(szCdrFile, pDirent->d_name))
            {
                return 0;
            }
        }
    }

    return -1;
}

int32_t SVS_SS_UTILITIES::ScanDir(const char *pszDirectory,
                    std::list<string> &vecFileList,
                    int32_t(*pFilterFunc)(const struct dirent *, void *),
                    void *pParamList)
{
    DIR *pDir = NULL;
    struct dirent *pDirp = NULL;
    char szDirNameTmp[256] = {0};

    pDir = opendir(pszDirectory);
    if(pDir == NULL)
    {
        SVS_LOG((SVS_LM_WARNING, "Can't open dir %s.", pszDirectory));
        return -1;
    }

    if(pszDirectory[strlen(pszDirectory)-1] == '/')
    {
        sprintf(szDirNameTmp, "%s", pszDirectory);
    }
    else
    {
        sprintf(szDirNameTmp, "%s/", pszDirectory);
    }

    while((pDirp = readdir(pDir)))
    {
        SVS_LOG((SVS_LM_DEBUG, "Read dir %s.", pDirp->d_name));

        if ((DUMMY_FILE_FILTER == pFilterFunc) || (0 == (*pFilterFunc)(pDirp, pParamList)))
        {
            char szTmpBuf[MAX_FILE_PATH_LEN] = {0};
            (void)snprintf(szTmpBuf, sizeof(szTmpBuf), "%s%s", szDirNameTmp, pDirp->d_name);
            vecFileList.push_back(szTmpBuf);
        }
    }

    closedir(pDir);
    return 0;
}

int32_t SVS_SS_UTILITIES::IsDirEqual(const char *pszDir, const char *pszCmpDir)
{
    //�ж��ļ��Ƿ�Ϊ��ͬ��INODE
    struct stat stStat, stStatCmp;
    int32_t iRet = stat(pszDir, &stStat);
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING, "Dir[%s] cannot stat.", pszDir));
        return -1;
    }
    iRet = stat(pszCmpDir, &stStatCmp);
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING, "Dir[%s] cannot stat.", pszCmpDir));
        return -1;
    }
    if (stStat.st_ino == stStatCmp.st_ino)
    {
        return 0;
    }
    return -1;
}

int32_t SVS_SS_UTILITIES::IsOverdueCdr(const struct dirent *pDirent,
                                           void *pParamList)
{
    if (NULL == pDirent || NULL == pParamList)
    {
        return -1;
    }

    int32_t iKeepDays = *(int32_t *)pParamList;
    //�жϱ��������Ƿ����һ�꣬���ڴ���һ��Ĳ����ж�
    if (iKeepDays < 0 || iKeepDays > 365)
    {
        return -1;
    }
    //���ٱ���һ��
    if (iKeepDays == 0)
    {
        iKeepDays = 1;
    }

    if (pDirent->d_type == DT_REG)
    {
        uint32_t uiCmsId, uiIndex;
        char szDate[9] = {0};
        char szTime[7] = {0};
        int32_t iRet = sscanf(pDirent->d_name, "%u_%08s_%06s_%03d",
                &uiCmsId, szDate, szTime, &uiIndex);
        if (4 == iRet)
        {
            char szCdrFile[MAX_FILE_PATH_LEN] = {0};
            (void)snprintf(szCdrFile, sizeof(szCdrFile),
                "%u_%s_%s_%.3d", uiCmsId, szDate, szTime, uiIndex);
            if (0 == strcmp(szCdrFile, pDirent->d_name))
            {
                time_t tCurrent;
                (void)time(&tCurrent);
                //tCurrent -= tCurrent % (24 * 60 * 60);
                //tCurrent += 24 * 60 * 60;
                tCurrent -= (iKeepDays * (24 * 60 * 60));
#ifndef TIME_CMP_VIA_VALUE
                char    szOverdueDate[15] = {0};
                (void)time2str(szOverdueDate, sizeof(szOverdueDate), tCurrent);
                SVS_LOG((SVS_LM_DEBUG, "Overdue date[%s].", szOverdueDate));
                if (strncmp(szOverdueDate, szDate, 8) > 0)
#else
                time_t  tFileDate;
                char    szFileDate[15] = {0};
                (void)snprintf(szFileDate, sizeof(szFileDate), "%s000000", szDate);
                tFileDate = str2time(szFileDate);

                //�������С�ڵ�ǰ����-���������ģ�����Ҫɾ��
                if (tFileDate < tCurrent)
#endif
                {
                    return 0;
                }
            }
        }
    }

    return -1;
}

int32_t SVS_SS_UTILITIES::IsPuLog(const struct dirent *pDirent, void *)
{
    if (NULL == pDirent)
    {
        return -1;
    }

    if (pDirent->d_type == DT_REG)
    {
        char szFilePrefix[6] = {0};
        char szDate[9] = {0};
        char szTime[7] = {0};
        int32_t iRet = sscanf(pDirent->d_name, "%05s_%08s_%06s",
                szFilePrefix, szDate, szTime);
        if (3 == iRet)
        {
            char szPuLogFile[MAX_FILE_PATH_LEN] = {0};
            (void)snprintf(szPuLogFile, sizeof(szPuLogFile),
                "PULOG_%s_%s", szDate, szTime);
            if (0 == strcmp(szPuLogFile, pDirent->d_name))
            {
                return 0;
            }
        }
    }

    return -1;
}

void SVS_SS_UTILITIES::bit_turn_on(uint16_t& flag, int32_t pos)
{
    flag |= 1 << pos;
}

void SVS_SS_UTILITIES::bit_turn_off(uint16_t& flag, int32_t pos)
{
    flag &= ~(1 << pos);
}

bool SVS_SS_UTILITIES::bit_on( uint16_t flag, int32_t pos)
{
    return flag & (1 << pos);
}

bool SVS_SS_UTILITIES::bit_off( uint16_t flag, int32_t pos)
{
    return !(flag & (1 << pos));
}

void SVS_SS_UTILITIES::flip_bit(uint16_t& flag, int32_t pos)
{
    if (bit_on(flag, pos))
    {
        bit_turn_off(flag, pos);
    }
    else
    {
        bit_turn_on(flag, pos);
    }
}


bool is_loopback_ip(const char *strIp)
{
    const char *strLoopIp = "127.0.0.1";

    return (0 == ACE_OS::strcmp(strLoopIp, strIp));
}

int32_t appexit()
{
    SVS_LOG((SVS_LM_WARNING, "system call error. process end."));
#ifndef WIN32
    (void)kill(ACE_OS::getpid(), SIGTERM);
    (void)sleep(10);
#endif
    exit(-1);
}

int32_t getlocalip(char *strBuf, int32_t nBufSize)
{
    char strHostName[1024];

    if (0 != gethostname(strHostName, sizeof(strHostName)))
    {
        printf("gethostname error: %d.\n", errno);
        return -1;
    }

    struct hostent *ht = gethostbyname(strHostName);
    if (NULL == ht)
    {
        printf("gethostbyname error: %d.\n", h_errno);
        return -1;
    }

    char **pptr = NULL;

    int32_t nCount = 0;
    char *ptr = strBuf;
    for (pptr = ht->h_addr_list; NULL != *pptr; pptr++)
    {
        uint8_t *pch = (uint8_t *)*pptr;

#ifndef WIN32
        int32_t n = snprintf(ptr, (size_t)nBufSize, "%d:%d:%d:%d", pch[0], pch[1], pch[2], pch[3]);
#else
        int32_t n = _snprintf(ptr, nBufSize, "%d:%d:%d:%d", pch[0], pch[1], pch[2], pch[3]);
#endif

        if (-1 == n)
        {
            printf("buffer is too small.\n");
            return -1;
        }

        n++;   //�����ս�����.
        nBufSize -= n;
        ptr += n;

        nCount++;
    }

    return nCount;
}


int32_t open_log_timestamp()
{
#ifndef WIN32
    const char *strName = "ACE_LOG_TIMESTAMP";
    const char *strExpectedValue = "DATE";
    const size_t nValueSize = strlen(strExpectedValue);

    char *pstrValue = getenv(strName);

    if (NULL != pstrValue)
    {
        //ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) environ[%s] value: %s.\n"), strName, pstrValue));
        if (0 == memcmp(strExpectedValue, pstrValue, nValueSize))
        {
            return 0;
        }
    }

    if (-1 == setenv(strName, strExpectedValue, 1))
    {
        SVS_LOG((SVS_LM_ERROR, "Setting environ[%s] error.", strName));
        return -1;
    }

#endif //WIN32
    //ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Setting environ[%s] OK.\n"), strName));

    return 0;
}

int32_t CallShellSendEMail(const char *strTitle, const char *strContent)
{
    char buf[1024];

    (void)ACE_OS::snprintf(buf,
                     sizeof(buf),
                     "/usr/local/SVS/FaultMail.sh \"%s\" \"%s\"",
                     strTitle,
                     strContent);

    SVS_LOG((SVS_LM_INFO, "%s", buf));

    if (-1 == system(buf))
    {
        SVS_LOG((SVS_LM_ERROR, "call system error."));
    }

    return 0;
}

int32_t digit_check(const char* str)
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

void SetLimitValue( int32_t type, int32_t newValue )
{
#ifndef WIN32
    rlimit rlim;
    rlimit rlim_new;
    if (0 == getrlimit(type, &rlim))
    {
        rlim_new.rlim_cur = rlim_new.rlim_max = (rlim_t)newValue;
        if (setrlimit(type, &rlim_new) != 0)
        {
            rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
            (void) setrlimit(type, &rlim_new);
            SVS_LOG((SVS_LM_WARNING, "set system resource limit failed! type[%d]", type));
        }
        else
        {
            SVS_LOG((SVS_LM_INFO, "set system resource limit success. type[%d], oldValue[%d], oldMaxValue[%d], newValue[%d], newMaxValue[%d].",
                       type,
                       rlim.rlim_cur,
                       rlim.rlim_max,
                       rlim_new.rlim_cur,
                       rlim_new.rlim_max));
        }
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING, "get system resource limit failed! type[%d]", type));
    }
#endif

}

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
int32_t getDebugConfig(const char *svrCfgFile, int32_t &runType, int32_t &logEnable, int32_t &logLM)
{
    CAC_Config config(svrCfgFile);
    ACE_TCHAR strTemp[32];

    if (config.get("SVS_DEBUG", "RunType", strTemp ) != 0)
    {
        SVS_LOG((SVS_LM_ERROR, "read config item [RunType] in section [SVS_LOG] failed."));
        return -1;
    }

    runType = atoi( strTemp );
    if ((0 != runType) && (1 != runType))
    {
        runType = 1;
    }

    if (config.get("SVS_DEBUG", "LogEnable", strTemp ) != 0)
    {
        SVS_LOG((SVS_LM_ERROR, "read config item [LogEnable] in section [SVS_LOG] failed."));
        return -1;
    }

    logEnable = atoi( strTemp );

    if (config.get("SVS_DEBUG", "LogLM", strTemp ) != 0)
    {
        SVS_LOG((SVS_LM_ERROR, "read config item [LogLM] in section [SVS_LOG] failed."));
        return -1;
    }

    logLM = atoi( strTemp );
    return 0;
}


// �ܶ���վ��ID�ĵط�Ҳ�����˴˺�����������֮
int32_t CheckDeviceID(const char *pszDevID)
{
    if (NULL == pszDevID)
    {
        return CHECK_ERROR;
    }

    int32_t iDeviceLen = strlen(pszDevID);

    if (iDeviceLen <= 0 || iDeviceLen > SVS_ID32_LEN)
    {
        return CHECK_ERROR;
    }

    for (int32_t i = 0; i < iDeviceLen; i++)
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

int32_t CheckDeviceIDEx(const char *pszDevID)
{
    if (NULL == pszDevID)
    {
        return CHECK_ERROR;
    }

    int32_t iDeviceLen = strlen(pszDevID);

    if ((iDeviceLen != LENSDEVID_LEN_C05) && (iDeviceLen != SVS_DEVICEID_LEN))
    {
        return CHECK_ERROR;
    }

    for (int32_t i = 0; i < iDeviceLen; i++)
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



