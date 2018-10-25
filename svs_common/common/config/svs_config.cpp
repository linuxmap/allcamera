// CAC_Config.cpp: implementation of the CAC_Config class.
//
//////////////////////////////////////////////////////////////////////

#include "svs_config.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


#define CheckCharIsEnterOrNewLine(c)   (c == 10 || c == 13)    //是否为回车或者新行
#define CheckCharIsSpaceOrTab(c)   (c == 32 || c == 9)             //是否为空格或者制表符

CAC_Config::CAC_Config(const char *fileName):_fLock(ACE_INVALID_HANDLE,0), _fName(fileName),_bLockReleased(false),_bFileClosed(false),_bufferSize(0),_buffer(0)
{
    if(_fLock.open(_fName,
            O_RDWR | O_CREAT,
            ACE_DEFAULT_FILE_PERMS) == -1)
    {
        return ;
    }

    _fNameTemp = NULL;
}

CAC_Config::~CAC_Config()
{
    try
    {
        (void)_fLock.remove(0);
    }
    catch(...){}

    _fName = NULL;
    _fNameTemp = NULL;
    _buffer = NULL;
}

char * CAC_Config::trim(char *pstr)const
{
    int32_t     len;
    char   *pRet;

    pRet = pstr;
    /*去掉首部空格*/
    while ( ((*pRet) == 32) || ((*pRet) == 9))
    {
        pRet ++;
    }

    /*去掉尾部空格*/
    len = (int32_t)strlen(pRet);
    while (len>0)
    {
        if ( (pRet[len-1] == 32) || (pRet[len-1] == 9) || (pRet[len-1] == 13) || (pRet[len-1] == 10) )
        {
            len --;
        }
        else
        {
            break;
        }
    }
    *(pRet + len) = 0;
    return pRet;
}

/*****************************************************************
 *    名称        : get
 *    功能        : 获取具体Key值,通过输出参数outKeyValue返回
 *    创建        : 2004-4-14 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : sectionName    - section名
 *                  keyName        - key名
 *    输出参数    : outKeyValue    - 返回的Key值
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */
int32_t CAC_Config::get(const char *sectionName, const char *keyName, char *outKeyValue, bool bRtrim, uint32_t unLen)
{
    /* 获取读锁. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    if (recv() != 0)
    {
        (void)free();
        (void)_fLock.release();
        return -1;
    }

    if (_bufferSize == 0)
    {
        (void)free();
        (void)_fLock.release();
        return -1;
    }

    /* 释放锁. */
    (void)_fLock.release();

    /* 进行信息提取. */

    *outKeyValue = '\0';

    int32_t ret = -1;

    //本段代码被提取成内部函数 inner_subFuncOfGetKey
    inner_subFuncOfGetKey(sectionName, keyName, outKeyValue, unLen, ret);

    (void)free();

    if (ACE_OS::strcmp(outKeyValue,"") == 0)
    {
        ret = -1;
    }

    if(bRtrim)
    {
        this->rtrim(outKeyValue);
    }

    return ret;

}

void CAC_Config::inner_subFuncOfGetKey(const char *sectionName, const char *keyName, char *outKeyValue, uint32_t unLen, int32_t & ret)
{
    uint32_t i = 0;
    bool bContinue = true;
    while(bContinue && i < _bufferSize)
    {
        /* 当碰到section时. */
        if (*(_buffer+i++) == '[')
        {
            bool    bSectionEnd = false;
            char    temp[50] = {0,};
            char    c;
            int32_t        j = 0;
            while(!bSectionEnd)
            {
                c = *(_buffer+i++);
                if (c != ']')
                {
                    temp[j++] = c;
                }
                else
                {
                    temp[j] = '\0';
                    bSectionEnd = true;
                }
            }

            /* 如果和参数sectionName一致,则检索key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                while (bContinue && i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* 用户搜索的section下面无key,所以直接返回. */
                    {
                        bContinue = false;
                        ret = -1;
                        break;
                    }
                    if (CheckCharIsEnterOrNewLine(c) || CheckCharIsSpaceOrTab(c)) /* 回车 - 13,  新行 - 10, 空格 - 32, Tab键 - 9 */
                    {
                        continue;
                    }
                    /* 遇到注释，则找到换行符. */
                    else if (c == CONFIG_NOTE_SYMBOL)
                    {
                        while (i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* 检索Key,并和输入参数keyName进行匹配. */
                        j = 0;
                        (void)ACE_OS::strcpy(temp,"");
                        bool bKeyFinded = false;
                        i--;
                        while (!bKeyFinded && i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                            else if (c == '=' || CheckCharIsSpaceOrTab(c))
                            {
                                temp[j] = '\0';
                                bKeyFinded = true;
                                break;
                            }
                            else
                            {
                                temp[j++] = c;
                            }
                        }/* end of while. */

                        /* 将key和输入参数keyName进行匹配. */
                        if (bKeyFinded)
                        {
                            if (ACE_OS::strcmp(temp,keyName) == 0) /* 匹配的情况下. */
                            {
                                ret = 0;
                                j = 0;
                                i--;
                                bool bKeyValueFinded = false;
                                while (!bKeyValueFinded && i < _bufferSize)
                                {
                                    c = *(_buffer + i++);
                                    if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL) /* 当Key的值的第一个字符为 "回车","新行","注释" 的情况. */
                                    {
                                        bContinue = false;
                                        ret =  -1;
                                        break;
                                    }
                                    else if (CheckCharIsSpaceOrTab(c)) /* 为"空格"和"Tab键"的情况. */
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        /* 正常情况下,获取key值. */
                                        i--;
                                        bool    bTemp = false;
                                        while ((i < _bufferSize)&&(j < (int32_t)unLen - 1))
                                        {
                                            c = *(_buffer+i++);
                                            if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL)
                                            {
                                                bKeyValueFinded = true;
                                                break;
                                            }
                                            else if (!bTemp && CheckCharIsSpaceOrTab(c))
                                            {
                                                continue;
                                            }
                                            else if (!bTemp &&(c == '='))
                                            {
                                                bTemp = true;
                                            }
                                            else
                                                *(outKeyValue + j++) = c;

                                        }

                                        *(outKeyValue + j) = '\0';
                                        (void)trim(outKeyValue);

                                        bContinue = false;
                                        break;
                                    }

                                }/* end of while. */
                            }
                            else        /* 不匹配的情况下,移动到最后. */
                            {
                                while (i < _bufferSize)
                                {
                                    c = *(_buffer+i++);
                                    if (CheckCharIsEnterOrNewLine(c))
                                    {
                                        break;
                                    }
                                }
                            }
                        }/* end of if. */
                    }
                }/* end of while. */
            }
        }
        /* 当该行是注释行时(用CONFIG_NOTE_SYMBOL开头). */
        else if (*(_buffer+i) == CONFIG_NOTE_SYMBOL)
        {
            while (i < _bufferSize)
            {
                i++;
                if (*(_buffer+i) == '\n')
                {
                    break;
                }
            }
        }

    }/* end of while. */
}

void CAC_Config::rtrim(char *str)const
{
    char *pch = strchr(str, ' ');
    if(NULL != pch)
    {
        *pch = '\0';
    }

    pch = strchr(str, '#');
    if(NULL != pch)
    {
        *pch = '\0';
    }

    pch = strchr(str, ';');
    if(NULL != pch)
    {
        *pch = '\0';
    }
}


/*****************************************************************
 *    名称        : get
 *    功能        : 获取Key列表,通过输出参数vect返回
 *    创建        : 2004-4-14 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : sectionName    - section名
 *    输出参数    : outKeyList    - 返回的Key列表
 *    返回值        : Key列表中参数个数 (如果为0,则不存在)
 *****************************************************************
 */
int32_t CAC_Config::get(const char *sectionName, ACE_Vector<GE_CONFIG_KEY_LIST> *vect, uint32_t unLen)
{
    /* 获取读锁. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    if (recv() != 0)
    {
        (void)free();
        (void)_fLock.release();
        return -1;
    }

    if (_bufferSize == 0)
    {
        (void)free();
        (void)_fLock.release();
        return -1;
    }


    /* 释放锁. */
    (void)_fLock.release();

    /* 进行信息提取. */

    //int32_t    index = 0;

    GE_CONFIG_KEY_LIST    note;

    int32_t ret = -1;
    size_t i = 0;
    char    temp[512] = {0,};
    char    c;
    bool    bContinue = true;

    while(bContinue && i < _bufferSize)
    {
        /* 当碰到section时. */
        c = *(_buffer+i++);
        if (c == '[')
        {
            //            bool    bSectionEnd = false;
            (void)ACE_OS::strcpy(temp,"");
            int32_t        j = 0;
            while((size_t)i < _bufferSize)
            {
                c = *(_buffer+i++);
                if (c != ']')
                {
                    temp[j++] = c;
                }
                else
                {
                    temp[j] = '\0';
                    break;
                }
            }

            /* 如果和参数sectionName一致,则检索key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                bool bKeyEnd = false;
                while (!bKeyEnd && (size_t)i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* 用户搜索的section下面无key,所以直接返回. */
                    {
                        bContinue = false;
                        break;

                    }
                    if (CheckCharIsEnterOrNewLine(c) || CheckCharIsSpaceOrTab(c) || c < 0) /* 回车 - 13,  新行 - 10, 空格 - 32, Tab键 - 9 */
                    {
                        continue;
                    }
                    /* 遇到注释，则找到换行符. */
                    else if (c == CONFIG_NOTE_SYMBOL)
                    {
                        while ((size_t)i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* 检索Key,并将Key名和值记录在输出参数vect中. */
                        ret = 0;
                        j = 0;
                        (void)ACE_OS::strcpy(temp,"");
                        bool bKeyFinded = false;
                        i--;
                        while (!bKeyFinded && (size_t)i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                            else if (c == '=' || CheckCharIsSpaceOrTab(c))
                            {
                                temp[j] = '\0';
                                bKeyFinded = true;
                                break;
                            }
                            else
                            {
                                temp[j++] = c;
                            }
                        }/* end of while. */

                        (void)ACE_OS::strcpy(note.KeyName,temp);
                        (void)ACE_OS::strcpy(note.KeyValue,"");

                        /* 将key和输入参数keyName进行匹配. */
                        if (bKeyFinded)
                        {
                            j = 0;
                            i--;
                            (void)ACE_OS::strcpy(temp,"");
                            bool bKeyValueFinded = false;
                            while (!bKeyValueFinded && (size_t)i < _bufferSize)
                            {
                                c = *(_buffer + i++);
                                if (CheckCharIsEnterOrNewLine(c)) /* 当Key的值的第一个字符为 "回车","新行" 的情况. */
                                {
                                    bKeyValueFinded = true;
                                    break;
                                }
                                else if (c == CONFIG_NOTE_SYMBOL)
                                {
                                    while ((size_t)i < _bufferSize)
                                    {
                                        c = *(_buffer+i++);
                                        if (CheckCharIsEnterOrNewLine(c))
                                        {
                                            break;
                                        }
                                    }
                                    bKeyValueFinded = true;
                                    break;
                                }
                                else if (CheckCharIsSpaceOrTab(c)) /* 为"空格"和"Tab键"的情况. */
                                {
                                    continue;
                                }
                                else
                                {
                                    /* 正常情况下,获取key值. */
                                    ret = 0;
                                    i--;
                                    bool    bTemp = true;
                                    while ((i < _bufferSize)&&(j < (int32_t)unLen - 1))
                                    {
                                        c = *(_buffer+i++);
                                        if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL)
                                        {
                                            bKeyValueFinded = true;
                                            break;
                                        }
                                        else if (!bTemp && CheckCharIsSpaceOrTab(c))
                                        {
                                            continue;
                                        }
                                        else if (!bTemp &&(c == '='))
                                        {
                                            bTemp = true;
                                        }
                                        else
                                        {
                                            temp[j++] = c;
                                        }
                                    }

                                    temp[j] = '\0';
                                    (void)trim(&temp[0]);
                                    (void)ACE_OS::strcpy(note.KeyValue,temp);
                                }

                            }/* end of while. */

                            /* 将获取的key名及值加载到参数vect中. */
                            vect->push_back(note);

                        }/* end of if. */
                    }
                }/* end of while. */
            }
        }
        /* 当该行是注释行时(用CONFIG_NOTE_SYMBOL开头). */
        else if (c == CONFIG_NOTE_SYMBOL)
        {
            while ((size_t)i < _bufferSize)
            {
                c = *(_buffer+i++);
                if (CheckCharIsEnterOrNewLine(c))
                {
                    break;
                }
            }
        }

        //else
        //    continue;

    }/* end of while. */

    (void)free();

    return ret;
}

/*****************************************************************
 *    名称        : set
 *    功能        : 设置配置
 *    创建        : 2004-4-16 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : sectionName
 *                  keyName
 *                  keyValue
 *    输出参数    : 无
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */
int32_t CAC_Config::set(const char *sectionName,const char *keyName, const char *keyValueFormat,...)
{

    /*
     *实现原理：
     *首先读取配置文件数据，然后进行检索，查询到指定的key。
     *然后标记key的位置.然后清空配置文件,并将数据分段插入到配置文件中.
     */

    /* 解析输入key值. */
    char keyValue[100];
    va_list ap;
    va_start( ap, keyValueFormat );

#if defined (ACE_WIN32)
    ::_vsnprintf( keyValue, sizeof( keyValue ), keyValueFormat, ap );
#else
    (void)::vsnprintf( keyValue, sizeof( keyValue ), keyValueFormat, ap );
#endif
    va_end( ap );

    /*
    *判断输入是否合法
    */

    if (ACE_OS::strcmp(sectionName,"") == 0 || ACE_OS::strcmp(keyName,"") == 0 || ACE_OS::strcmp(keyValue,"") == 0)
    {
        return -1;
    }

    /* 获取读锁,并打开文件,获取数据. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* 释放锁. */
    (void)_fLock.release();

    /************************************************************************/
    /*                            检索配置数据                                */
    /************************************************************************/
    FUNC_SET_INNER__PARAM innerParam = {0};
    innerParam.index1 = 0;
    innerParam.index2 = 0;
    innerParam.bContinue = true;
    innerParam.bSectionMatched = false;
    innerParam.bKeyMatched = false;

    //本段代码被提取成内部函数 inner_subFuncOfSet
    inner_subFuncOfSet(sectionName, keyName, innerParam);


    /************************************************************************/
    /*                        生成临时配置文件                                */
    /************************************************************************/


    /*
     *    1. 当用户检索的key存在时.
     *
     *    两个innerParam.index来记录key值的长度及位置.
     *    然后将_buffer的数据写入到临时文件中，并修改key值.
     */

    char *tempBuf;
    size_t    size;
    char c;

    if (innerParam.bKeyMatched)
    {
        innerParam.index2--;
        while(innerParam.index2 > 0)
        {
            c = *(_buffer + innerParam.index2--);
            if (c == 13 || c == 10 || c == 9)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        innerParam.index2++;

        (void)_fLock.acquire_write();

        _fIO.set_handle(_fLock.get_handle());

        /* 清空配置文件. */
        (void)_fIO.truncate(0);

        /* 发送第一个数据块(共三块). */
        (void)_fIO.seek(0,SEEK_SET);
        size = (size_t)innerParam.index1;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer,innerParam.index1);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;
        tempBuf = 0;

        /* 发送第二个数据块(共三块). */
        size = ACE_OS::strlen(keyValue);
        char    t[100];
        (void)ACE_OS::sprintf(t,"%s",keyValue);
        (void)_fIO.send(t,size);

        /* 发送第三个数据块(共三块). */
        innerParam.index2++;
        size = _bufferSize - innerParam.index2;

        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer+innerParam.index2,size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        (void)_fLock.release();
    }

    /*
    *2. 当用户检索的section存在,但是key不存在时,则要插入新key.
    *
    *    innerParam.index1记录了需要插入新key的位置.
    */

    else if (innerParam.bSectionMatched)
    {
        innerParam.index2--;
        innerParam.index2--; //char u = *(_buffer+innerParam.index2--);
        while(innerParam.index2 > 0)
        {
            c = *(_buffer + innerParam.index2--);
            if (c == 13 || c == 10)
            {
                continue;
            }
            else
                break;
        }

        innerParam.index2 = innerParam.index2 + 2;

        /* 打开临时文件,如果不存在则创建. */


        (void)_fLock.acquire();

        _fIO.set_handle(_fLock.get_handle());

        (void)_fIO.truncate(0);

        /* 发送第一个数据块(共三块). */
        (void)_fIO.seek(0);
        size = innerParam.index2;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer, innerParam.index2);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        /* 发送第二个数据块(共三块). */
        (void)_fIO.seek(0);
        size = ACE_OS::strlen(keyName) + ACE_OS::strlen(keyValue) + 3;

        char t[100];
        (void)ACE_OS::sprintf(t,"%c%c%s=%s",13,10,keyName,keyValue);
        (void)_fIO.send(t,size);

        /* 发送第三个数据块(共三块). */
        (void)_fIO.seek(0);
        size = _bufferSize - innerParam.index2;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer+innerParam.index2,size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        (void)_fLock.release();
    }


    /*
    *3. 当用户检索的section不存在时,则要插入新section和新key
    *
    *在配置文件最后插入相应配置参数.
    */

    else
    {
        (void)_fLock.acquire_write();

        _fIO.set_handle(_fLock.get_handle());

        size = ACE_OS::strlen(sectionName) + ACE_OS::strlen(keyName) + ACE_OS::strlen(keyValue) + 9; /*包括三个字符10 和三个字符 13 和'[',']','='*/


        /* 在配置文件末尾添加参数配置. */

        (void)_fIO.seek(0,SEEK_END);
        char    t[100] = {0,};

        if (_bufferSize == 0)
        {
            size = size -2;
            (void)ACE_OS::snprintf(t,size<=100?size:100,"[%s]%c%c%s=%s%c%c",sectionName,13,10,keyName,keyValue,13,10);
        }
        else
        {
            (void)ACE_OS::snprintf(t,size<=100?size:100,"%c%c[%s]%c%c%s=%s%c%c",13,10,sectionName,13,10,keyName,keyValue,13,10);
        }

        (void)_fIO.send(t,size);

        (void)_fLock.release();
    }

    (void)free();

    return 0;
}

void CAC_Config::inner_subFuncOfSet(const char *sectionName,const char *keyName, FUNC_SET_INNER__PARAM & innerParam)
{
    size_t i = 0;
    char    c;
    while(innerParam.bContinue && (size_t)i < _bufferSize)
    {
        /* 当碰到section时. */
        c = *(_buffer+i++);
        if (c == '[')
        {
            bool    bSectionEnd = false;
            char    temp[50] = {0,};
            int32_t        j = 0;
            while(!bSectionEnd)
            {
                c = *(_buffer+i++);
                if (c != ']')
                {
                    temp[j++] = c;
                }
                else
                {
                    temp[j] = '\0';
                    bSectionEnd = true;
                }
            }

            /* 如果和参数sectionName一致,则检索key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                innerParam.bSectionMatched = true;
                while (innerParam.bContinue && (size_t)i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* 用户搜索的section下面无key,所以跳出循环. */
                    {
                        innerParam.index1 = i;
                        innerParam.bContinue = false;
                        break;
                    }
                    if (CheckCharIsEnterOrNewLine(c)|| CheckCharIsSpaceOrTab(c)) /* 回车 - 13,  新行 - 10, 空格 - 32, Tab键 - 9 */
                    {
                        continue;
                    }
                    /* 遇到注释，则找到换行符. */
                    else if (c == CONFIG_NOTE_SYMBOL)
                    {
                        while ((size_t)i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* 检索Key名. */
                        j = 0;
                        (void)ACE_OS::strcpy(temp,"");
                        bool bKeyFinded = false;
                        i--;
                        while (!bKeyFinded && (size_t)i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                            else if (c == '=')
                            {
                                innerParam.index1 = i;
                                temp[j] = '\0';
                                bKeyFinded = true;
                                break;
                            }
                            else
                                temp[j++] = c;

                        }/* end of while. */

                        /* 将key和输入参数keyName进行匹配. */
                        if (bKeyFinded)
                        {
                            (void)trim(temp);
                            if (ACE_OS::strcmp(temp,keyName) == 0) /* 匹配的情况下. */
                            {
                                innerParam.bKeyMatched = true;
                                j = 0;
                                bool bKeyValueFinded = false;
                                while (!bKeyValueFinded && (size_t)i < _bufferSize)
                                {
                                    c = *(_buffer + i++);
                                    if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL) /* 当Key的值的第一个字符为 "回车","新行","注释" 的情况. */
                                    {
                                        innerParam.index2 = i - 2;
                                        innerParam.bContinue = false;
                                        break;
                                    }
                                    else if (CheckCharIsSpaceOrTab(c)) /* 为"空格"和"Tab键"的情况. */
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        /* 正常情况下,获取key值. */
                                        i--;
                                        while (innerParam.bContinue && (size_t)i < _bufferSize)
                                        {
                                            c = *(_buffer+i++);
                                            if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL)
                                            {
                                                bKeyValueFinded = true;
                                                break;
                                            }

                                        }

                                        innerParam.index2 = i - 1;
                                        innerParam.bContinue = false;
                                    }

                                }/* end of while. */
                            }
                            else    /* 不匹配的情况下,移动到最后. */
                            {
                                while ((size_t)i < _bufferSize)
                                {
                                    c = *(_buffer+i++);
                                    if (CheckCharIsEnterOrNewLine(c))
                                    {
                                        break;
                                    }
                                }
                            }
                        }/* end of if. */
                    }
                }/* end of while(search key). */

                if (!innerParam.bKeyMatched)
                    innerParam.index2 = i - 1;
            }/* end of if(section matched). */
        }
        /* 当该行是注释行时(用CONFIG_NOTE_SYMBOL开头). */
        else if (c == CONFIG_NOTE_SYMBOL)
        {
            while ((size_t)i < _bufferSize)
            {
                c = *(_buffer+i++);
                if (CheckCharIsEnterOrNewLine(c))
                {
                    break;
                }
            }
        }
        else
            continue;

    }/* end of while. */
}


/*****************************************************************
 *    名称        : del
 *    功能        : 删除配置,删除指定section下面所有key
 *    创建        : 2004-4-16 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : sectionName
 *    输出参数    : 无
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */

int32_t CAC_Config::del(const char *sectionName)
{
    /* 获取读锁,并打开文件,获取数据. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* 释放锁. */
    (void)_fLock.release();

    /************************************************************************/
    /*                          检索配置数据                                */
    /************************************************************************/

    int32_t i = 0;//, k = 0;
    int32_t index1 = 0, index2 = 0;
    char    c;
    bool    bContinue        = true;
    bool    bSectionMatched = false;

    while(bContinue && (size_t)i < _bufferSize)
    {
        /* 当碰到section时. */
        c = *(_buffer+i++);
        if (c == '[')
        {
            if (bSectionMatched)
            {
                index2 = i - 1;
            }
            else
            {
                bool    bSectionEnd = false;
                char    temp[50] = {0};
                int32_t        j = 0;
                while(!bSectionEnd)
                {
                    c = *(_buffer+i++);
                    if (c != ']')
                    {
                        temp[j++] = c;
                    }
                    else
                    {
                        temp[j] = '\0';
                        bSectionEnd = true;
                    }
                }

                /* 如果和参数sectionName一致,则检索key. */
                (void)trim(temp);
                if (ACE_OS::strcmp(temp,sectionName) == 0)
                {
                    index1 = i - 1;
                    bSectionMatched = true;

                }/* end of if(section matched). */
            }
        }
        /* 当该行是注释行时(用CONFIG_NOTE_SYMBOL开头). */
        else if (c == CONFIG_NOTE_SYMBOL)
        {
            while ((size_t)i < _bufferSize)
            {
                c = *(_buffer+i++);
                if (CheckCharIsEnterOrNewLine(c))
                {
                    break;
                }
            }
        }
        else
            continue;

    }/* end of while. */


    /************************************************************************/
    /*                        重新生成配置文件                                */
    /************************************************************************/

    if (!bSectionMatched)
    {
        (void)free();
        return -1;
    }

    for(i = index1; i >= 0 ;i--)
    {
        c = *(_buffer + i);
        if (c == 13)
        {
            break;
        }
    }

    index1 = i;

    index2--;
    for(i = index2; i >= 0 ;i--)
    {
        c = *(_buffer + i);
        if (c == 13 || c == 10)
        {
            continue;
        }
        else
        {
            break;
        }
    }

    index2 = i + 1;

    /* 去除index1和index2之间的所有数据.将前面和后面的数据加到配置文件中. */

    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    /* 清空配置文件. */
    (void)_fIO.truncate(0);

    /* 发送第一个数据块(共两块). */
    (void)_fIO.seek(0,SEEK_SET);

    size_t size = (size_t)index1;
    char *tempBuf = new char[size];
    (void)ACE_OS::strncpy(tempBuf,_buffer,size);
    (void)_fIO.send(tempBuf,size);
    delete [] tempBuf;
    tempBuf = 0;

    /* 发送第二个数据块(共两块). */

    size = _bufferSize - (size_t)index2;

    tempBuf = new char[size];
    (void)ACE_OS::strncpy(tempBuf,_buffer+index2,size);
    (void)_fIO.send(tempBuf,size);
    delete [] tempBuf;

    (void)_fLock.release();
    (void)free();

    return 0;
}

/*****************************************************************
 *    名称        : del
 *    功能        : 删除配置,删除指定section下面指定的key
 *    创建        : 2004-4-16 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : sectionName
 *                  keyName
 *    输出参数    : 无
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */
int32_t CAC_Config::del(const char *sectionName, const char *keyName)
{
    /* 获取读锁,并打开文件,获取数据. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* 释放锁. */
    (void)_fLock.release();

    /************************************************************************/
    /*                            检索配置数据                                */
    /************************************************************************/

    int32_t i = 0; //, k = 0;
    int32_t index2 = 0;
    char    c;
    bool    bContinue        = true;
    bool    bSectionMatched = false;
    bool    bKeyMatched        = false;

    while(bContinue && (size_t)i < _bufferSize)
    {
        /* 当碰到section时. */
        c = *(_buffer+i++);
        if (c == '[')
        {
            bool    bSectionEnd = false;
            char    temp[50] = {0,};
            int32_t        j = 0;
            while(!bSectionEnd)
            {
                c = *(_buffer+i++);
                if (c != ']')
                {
                    temp[j++] = c;
                }
                else
                {
                    temp[j] = '\0';
                    bSectionEnd = true;
                }
            }

            /* 如果和参数sectionName一致,则检索key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                bSectionMatched = true;
                while (bContinue && (size_t)i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* 用户搜索的section下面无key,所以跳出循环. */
                    {
                        bContinue = false;
                        break;
                    }
                    if (CheckCharIsEnterOrNewLine(c)|| CheckCharIsSpaceOrTab(c)) /* 回车 - 13,  新行 - 10, 空格 - 32, Tab键 - 9 */
                    {
                        continue;
                    }
                    /* 遇到注释，则找到换行符. */
                    else if (c == CONFIG_NOTE_SYMBOL)
                    {
                        while ((size_t)i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        /* 检索Key名. */
                        j = 0;
                        (void)ACE_OS::strcpy(temp,"");
                        bool bKeyFinded = false;
                        i--;
                        while (!bKeyFinded && (size_t)i < _bufferSize)
                        {
                            c = *(_buffer+i++);
                            if (CheckCharIsEnterOrNewLine(c))
                            {
                                break;
                            }
                            else if (c == '=' || CheckCharIsSpaceOrTab(c))
                            {
                                temp[j] = '\0';
                                bKeyFinded = true;
                                break;
                            }
                            else
                            {
                                temp[j++] = c;
                            }
                        }/* end of while. */

                        /* 将key和输入参数keyName进行匹配. */
                        if (bKeyFinded)
                        {
                            if (ACE_OS::strcmp(temp,keyName) == 0) /* 匹配的情况下. */
                            {
                                bKeyMatched = true;
                                index2 = i;
                                bContinue = false;

                            }
                            else    /* 不匹配的情况下,移动到最后. */
                            {
                                while ((size_t)i < _bufferSize)
                                {
                                    c = *(_buffer+i++);
                                    if (CheckCharIsEnterOrNewLine(c))
                                    {
                                        break;
                                    }
                                }
                            }
                        }/* end of if. */
                    }
                }/* end of while(search key). */

                if (!bKeyMatched)
                    index2 = i - 1;
            }/* end of if(section matched). */
        }
        /* 当该行是注释行时(用CONFIG_NOTE_SYMBOL开头). */
        else if (c == CONFIG_NOTE_SYMBOL)
        {
            while ((size_t)i < _bufferSize)
            {
                c = *(_buffer+i++);
                if (CheckCharIsEnterOrNewLine(c))
                {
                    break;
                }
            }
        }
        else
            continue;

    }/* end of while. */

    if (!bSectionMatched)
    {
        (void)free();
        return -1;
    }

    if (!bKeyMatched)
    {
        (void)free();
        return -1;
    }

    /* 找到指定key的行头. */
    for(i = index2; i >= 0 ;i--)
    {
        c = *(_buffer + i);
        if (c == 13)
        {
            break;
        }
        else
            continue;
    }
    int32_t headIndex = i;

    /* 找到指定key的行尾. */
    for(i = index2; (size_t)i <= _bufferSize ;i++)
    {
        c = *(_buffer + i);
        if (CheckCharIsEnterOrNewLine(c))
        {
            break;
        }
    }

    int32_t endIndex = i;

    /************************************************************************/
    /*                        重新生成配置文件                                */
    /************************************************************************/

    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    /* 清空配置文件. */
    (void)_fIO.truncate(0);

    /* 发送第一个数据块(共三块). */
    (void)_fIO.seek(0,SEEK_SET);

    size_t size = (size_t)headIndex;
    char *tempBuf = new char[size];
    (void)ACE_OS::strncpy(tempBuf,_buffer,size);
    (void)_fIO.send(tempBuf,size);
    delete [] tempBuf;
    tempBuf = 0;

    /* 发送第二个数据块(共二块). */

    size = _bufferSize - (size_t)endIndex;

    tempBuf = new char[size];
    (void)ACE_OS::strncpy(tempBuf,_buffer+endIndex,size);
    (void)_fIO.send(tempBuf,size);
    delete [] tempBuf;

    (void)_fLock.release();
    (void)free();

    return 0;
}
/*****************************************************************
 *    名称        : recv
 *    功能        : 获取数据,存于_buffer中.
 *    创建        : 2004-4-16 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : 无
 *    输出参数    : 无
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */
int32_t CAC_Config::recv()
{
    /* 获取配置文件信息. */
    ACE_FILE_Info fInfo;

    if (_fIO.get_info (fInfo) == -1)
    {
        return -1;
    }

    if (fInfo.size_ <= 0)
    {
        return -1;
    }

    _bufferSize = (size_t)fInfo.size_;  /*信息长度*/

    try
    {
        _buffer = new char [_bufferSize];
    }
    catch(...)
    {
        return -1;
    }

    /* Make sure <buffer> is released automagically. */
    //ACE_Auto_Basic_Array_Ptr<char> b (_buffer);

    /* Move the file pointer back to the beginning of the file. */
    if (_fIO.seek (0,SEEK_SET) == -1)
    {
    }

    /* Read the cdr data from the file into the buffer. */
    size_t size = (size_t)_fIO.recv (_buffer, _bufferSize);
    if (size != _bufferSize)
    {
        return -1;
    }


    return 0;

}

/*****************************************************************
 *    名称        : free
 *    功能        : 释放内存
 *    创建        : 2004-4-22 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : 无
 *    输出参数    : 无
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */
int32_t CAC_Config::free()
{
    if (_buffer != 0)
    {
        delete [] _buffer;
        _buffer = 0;
    }

    return 0;
}

/*****************************************************************
 *    名称        : backup
 *    功能        : 备份配置文件
 *    创建        : 2004-4-22 by wzg@szhtp.com
 *    更改        :
 *    输入参数    : backupFileName    - 备份文件名
 *    输出参数    : 无
 *    返回值        : 成功 - 0
 *                失败 - -1
 *****************************************************************
 */
int32_t CAC_Config::backup(const char *backupFileName)
{
    /* 打开配置文件并读取数据. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    (void)_fLock.release();

    /* 创建备份文件,并将数据写入. */
    ACE_File_Lock    fLock2;

    if(fLock2.open(backupFileName,
        O_RDWR | O_CREAT,ACE_DEFAULT_FILE_PERMS) == -1)
    {
        (void)free();
        return -1;
    }

    (void)fLock2.acquire_write();

    ACE_FILE_IO        fIO2;

    fIO2.set_handle(fLock2.get_handle());

    (void)fIO2.truncate(0);
    (void)fIO2.send(_buffer,_bufferSize);

    (void)fLock2.release();

    (void)free();

    return 0;

}

int32_t CAC_Config::set_section(const char *old_section, const char *new_section)
{
    /*
    *判断输入是否合法
    */

    if (ACE_OS::strcmp(old_section,"") == 0)
    {
        return -1;
    }

    if (ACE_OS::strcmp(new_section,"") == 0)
    {
        return -1;
    }


    /* 获取读锁,并打开文件,获取数据. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* 释放锁. */
    (void)_fLock.release();
    //_fLock.remove(0);

    /************************************************************************/
    /*                            检索配置数据                                */
    /************************************************************************/

    int32_t i = 0; //, k = 0;
    int32_t    ret = -1;

    int32_t index1 = 0, index2 = 0;
    //    char    temp[50] = {'0'};
    char    c;
    bool    bContinue        = true;
    bool    bSectionMatched = false;
    //    bool    bKeyMatched        = false;

    while(bContinue && (size_t)i < _bufferSize)
    {
        /* 当碰到section时. */
        c = *(_buffer+i++);
        if (c == '[')
        {
            index1 = i;
            bool    bSectionEnd = false;
            char    temp[50] = {0,};
            int32_t        j = 0;
            while(!bSectionEnd)
            {
                c = *(_buffer+i++);
                if (c != ']')
                {
                    temp[j++] = c;
                }
                else
                {
                    temp[j] = '\0';
                    bSectionEnd = true;
                }
            }
            index2 = i-1;

            /* 如果和参数sectionName一致,则检索key. */
            if (ACE_OS::strcmp(temp,old_section) == 0)
            {
                bSectionMatched = true;
                bContinue        = false;

            }/* end of if(section matched). */

        }
        /* 当该行是注释行时(用GE_CONFIG_NOTE_SYMBOL开头). */
        else if (c == CONFIG_NOTE_SYMBOL)
        {
            while ((size_t)i < _bufferSize)
            {
                c = *(_buffer+i++);
                if (CheckCharIsEnterOrNewLine(c))
                {
                    break;
                }
            }
        }
        else
            continue;

    }/* end of while. */


    /************************************************************************/
    /*                        生成临时配置文件                                */
    /************************************************************************/


    /*
    *1. 当用户检索的key存在时.
    *
    *两个index来记录key值的长度及位置.
    *然后将_buffer的数据写入到临时文件中，并修改key值.
    */


    if (bSectionMatched)
    {
        ret = 0;
        size_t    size;
        char    *tempBuf;

        (void)_fLock.acquire_write();

        _fIO.set_handle(_fLock.get_handle());

        /* 清空配置文件. */
        (void)_fIO.truncate(0);

        /* 发送第一个数据块(共三块). */
        (void)_fIO.seek(0,SEEK_SET);
        size = (size_t)index1;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer, size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;
        tempBuf = 0;

        /* 发送第二个数据块(共三块). */
        size = ACE_OS::strlen(new_section);
        char    t[100];
        (void)ACE_OS::sprintf(t,"%s",new_section);
        (void)_fIO.send(t,size);

        /* 发送第三个数据块(共三块). */
        size = _bufferSize - (size_t)index2;

        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer+index2,size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        (void)_fLock.release();
    }
    else
    {
        ret = -1;
    }

    (void)free();

    return ret;
}

int32_t CAC_Config::add_log(const char *log_, ...)
{
    char strLog[100];
    va_list ap;
    va_start( ap, log_ );

#if defined (ACE_WIN32)
    ::_vsnprintf( strLog, sizeof( strLog ), log_, ap );
#else
    (void)::vsnprintf( strLog, sizeof( strLog ), log_, ap );
#endif
    va_end( ap );

    /* 获取读锁,并打开文件,获取数据. */
    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    (void)_fIO.seek(0,SEEK_END);

    size_t    size = ACE_OS::strlen(strLog);
    char    *buf;
    buf = new char[size + 2];

    (void)ACE_OS::sprintf(buf,"%s%c%c",strLog,13,10);

    (void)_fIO.send(buf,size+2);

    delete [] buf;

    /* 释放锁. */
    (void)_fLock.release();

    return 0;
}

int32_t CAC_Config::add_record_log(const char *log_, ...)
{
    char strLog[100] = {0};
    va_list ap;
    va_start( ap, log_ );

#if defined (ACE_WIN32)
    ::_vsnprintf( strLog, sizeof( strLog ), log_, ap );
#else
    (void)::vsnprintf( strLog, sizeof( strLog ), log_, ap );
#endif
    va_end( ap );

    /* 获取读锁,并打开文件,获取数据. */
    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    (void)_fIO.seek(0,SEEK_END);

    size_t    size = ACE_OS::strlen(strLog);
    char    *buf;
    buf = new char[size + 2];

    (void)ACE_OS::sprintf(buf,"%s%c%c",strLog,13,10);

    (void)_fIO.send(buf,size+2);

    delete [] buf;

    /* 释放锁. */
    (void)_fLock.release();

    return 0;
}



