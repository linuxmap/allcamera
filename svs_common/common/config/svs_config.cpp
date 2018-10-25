// CAC_Config.cpp: implementation of the CAC_Config class.
//
//////////////////////////////////////////////////////////////////////

#include "svs_config.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


#define CheckCharIsEnterOrNewLine(c)   (c == 10 || c == 13)    //�Ƿ�Ϊ�س���������
#define CheckCharIsSpaceOrTab(c)   (c == 32 || c == 9)             //�Ƿ�Ϊ�ո�����Ʊ��

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
    /*ȥ���ײ��ո�*/
    while ( ((*pRet) == 32) || ((*pRet) == 9))
    {
        pRet ++;
    }

    /*ȥ��β���ո�*/
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
 *    ����        : get
 *    ����        : ��ȡ����Keyֵ,ͨ���������outKeyValue����
 *    ����        : 2004-4-14 by wzg@szhtp.com
 *    ����        :
 *    �������    : sectionName    - section��
 *                  keyName        - key��
 *    �������    : outKeyValue    - ���ص�Keyֵ
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
 *****************************************************************
 */
int32_t CAC_Config::get(const char *sectionName, const char *keyName, char *outKeyValue, bool bRtrim, uint32_t unLen)
{
    /* ��ȡ����. */
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

    /* �ͷ���. */
    (void)_fLock.release();

    /* ������Ϣ��ȡ. */

    *outKeyValue = '\0';

    int32_t ret = -1;

    //���δ��뱻��ȡ���ڲ����� inner_subFuncOfGetKey
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
        /* ������sectionʱ. */
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

            /* ����Ͳ���sectionNameһ��,�����key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                while (bContinue && i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* �û�������section������key,����ֱ�ӷ���. */
                    {
                        bContinue = false;
                        ret = -1;
                        break;
                    }
                    if (CheckCharIsEnterOrNewLine(c) || CheckCharIsSpaceOrTab(c)) /* �س� - 13,  ���� - 10, �ո� - 32, Tab�� - 9 */
                    {
                        continue;
                    }
                    /* ����ע�ͣ����ҵ����з�. */
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
                        /* ����Key,�����������keyName����ƥ��. */
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

                        /* ��key���������keyName����ƥ��. */
                        if (bKeyFinded)
                        {
                            if (ACE_OS::strcmp(temp,keyName) == 0) /* ƥ��������. */
                            {
                                ret = 0;
                                j = 0;
                                i--;
                                bool bKeyValueFinded = false;
                                while (!bKeyValueFinded && i < _bufferSize)
                                {
                                    c = *(_buffer + i++);
                                    if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL) /* ��Key��ֵ�ĵ�һ���ַ�Ϊ "�س�","����","ע��" �����. */
                                    {
                                        bContinue = false;
                                        ret =  -1;
                                        break;
                                    }
                                    else if (CheckCharIsSpaceOrTab(c)) /* Ϊ"�ո�"��"Tab��"�����. */
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        /* ���������,��ȡkeyֵ. */
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
                            else        /* ��ƥ��������,�ƶ������. */
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
        /* ��������ע����ʱ(��CONFIG_NOTE_SYMBOL��ͷ). */
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
 *    ����        : get
 *    ����        : ��ȡKey�б�,ͨ���������vect����
 *    ����        : 2004-4-14 by wzg@szhtp.com
 *    ����        :
 *    �������    : sectionName    - section��
 *    �������    : outKeyList    - ���ص�Key�б�
 *    ����ֵ        : Key�б��в������� (���Ϊ0,�򲻴���)
 *****************************************************************
 */
int32_t CAC_Config::get(const char *sectionName, ACE_Vector<GE_CONFIG_KEY_LIST> *vect, uint32_t unLen)
{
    /* ��ȡ����. */
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


    /* �ͷ���. */
    (void)_fLock.release();

    /* ������Ϣ��ȡ. */

    //int32_t    index = 0;

    GE_CONFIG_KEY_LIST    note;

    int32_t ret = -1;
    size_t i = 0;
    char    temp[512] = {0,};
    char    c;
    bool    bContinue = true;

    while(bContinue && i < _bufferSize)
    {
        /* ������sectionʱ. */
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

            /* ����Ͳ���sectionNameһ��,�����key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                bool bKeyEnd = false;
                while (!bKeyEnd && (size_t)i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* �û�������section������key,����ֱ�ӷ���. */
                    {
                        bContinue = false;
                        break;

                    }
                    if (CheckCharIsEnterOrNewLine(c) || CheckCharIsSpaceOrTab(c) || c < 0) /* �س� - 13,  ���� - 10, �ո� - 32, Tab�� - 9 */
                    {
                        continue;
                    }
                    /* ����ע�ͣ����ҵ����з�. */
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
                        /* ����Key,����Key����ֵ��¼���������vect��. */
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

                        /* ��key���������keyName����ƥ��. */
                        if (bKeyFinded)
                        {
                            j = 0;
                            i--;
                            (void)ACE_OS::strcpy(temp,"");
                            bool bKeyValueFinded = false;
                            while (!bKeyValueFinded && (size_t)i < _bufferSize)
                            {
                                c = *(_buffer + i++);
                                if (CheckCharIsEnterOrNewLine(c)) /* ��Key��ֵ�ĵ�һ���ַ�Ϊ "�س�","����" �����. */
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
                                else if (CheckCharIsSpaceOrTab(c)) /* Ϊ"�ո�"��"Tab��"�����. */
                                {
                                    continue;
                                }
                                else
                                {
                                    /* ���������,��ȡkeyֵ. */
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

                            /* ����ȡ��key����ֵ���ص�����vect��. */
                            vect->push_back(note);

                        }/* end of if. */
                    }
                }/* end of while. */
            }
        }
        /* ��������ע����ʱ(��CONFIG_NOTE_SYMBOL��ͷ). */
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
 *    ����        : set
 *    ����        : ��������
 *    ����        : 2004-4-16 by wzg@szhtp.com
 *    ����        :
 *    �������    : sectionName
 *                  keyName
 *                  keyValue
 *    �������    : ��
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
 *****************************************************************
 */
int32_t CAC_Config::set(const char *sectionName,const char *keyName, const char *keyValueFormat,...)
{

    /*
     *ʵ��ԭ��
     *���ȶ�ȡ�����ļ����ݣ�Ȼ����м�������ѯ��ָ����key��
     *Ȼ����key��λ��.Ȼ����������ļ�,�������ݷֶβ��뵽�����ļ���.
     */

    /* ��������keyֵ. */
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
    *�ж������Ƿ�Ϸ�
    */

    if (ACE_OS::strcmp(sectionName,"") == 0 || ACE_OS::strcmp(keyName,"") == 0 || ACE_OS::strcmp(keyValue,"") == 0)
    {
        return -1;
    }

    /* ��ȡ����,�����ļ�,��ȡ����. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* �ͷ���. */
    (void)_fLock.release();

    /************************************************************************/
    /*                            ������������                                */
    /************************************************************************/
    FUNC_SET_INNER__PARAM innerParam = {0};
    innerParam.index1 = 0;
    innerParam.index2 = 0;
    innerParam.bContinue = true;
    innerParam.bSectionMatched = false;
    innerParam.bKeyMatched = false;

    //���δ��뱻��ȡ���ڲ����� inner_subFuncOfSet
    inner_subFuncOfSet(sectionName, keyName, innerParam);


    /************************************************************************/
    /*                        ������ʱ�����ļ�                                */
    /************************************************************************/


    /*
     *    1. ���û�������key����ʱ.
     *
     *    ����innerParam.index����¼keyֵ�ĳ��ȼ�λ��.
     *    Ȼ��_buffer������д�뵽��ʱ�ļ��У����޸�keyֵ.
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

        /* ��������ļ�. */
        (void)_fIO.truncate(0);

        /* ���͵�һ�����ݿ�(������). */
        (void)_fIO.seek(0,SEEK_SET);
        size = (size_t)innerParam.index1;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer,innerParam.index1);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;
        tempBuf = 0;

        /* ���͵ڶ������ݿ�(������). */
        size = ACE_OS::strlen(keyValue);
        char    t[100];
        (void)ACE_OS::sprintf(t,"%s",keyValue);
        (void)_fIO.send(t,size);

        /* ���͵��������ݿ�(������). */
        innerParam.index2++;
        size = _bufferSize - innerParam.index2;

        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer+innerParam.index2,size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        (void)_fLock.release();
    }

    /*
    *2. ���û�������section����,����key������ʱ,��Ҫ������key.
    *
    *    innerParam.index1��¼����Ҫ������key��λ��.
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

        /* ����ʱ�ļ�,����������򴴽�. */


        (void)_fLock.acquire();

        _fIO.set_handle(_fLock.get_handle());

        (void)_fIO.truncate(0);

        /* ���͵�һ�����ݿ�(������). */
        (void)_fIO.seek(0);
        size = innerParam.index2;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer, innerParam.index2);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        /* ���͵ڶ������ݿ�(������). */
        (void)_fIO.seek(0);
        size = ACE_OS::strlen(keyName) + ACE_OS::strlen(keyValue) + 3;

        char t[100];
        (void)ACE_OS::sprintf(t,"%c%c%s=%s",13,10,keyName,keyValue);
        (void)_fIO.send(t,size);

        /* ���͵��������ݿ�(������). */
        (void)_fIO.seek(0);
        size = _bufferSize - innerParam.index2;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer+innerParam.index2,size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;

        (void)_fLock.release();
    }


    /*
    *3. ���û�������section������ʱ,��Ҫ������section����key
    *
    *�������ļ���������Ӧ���ò���.
    */

    else
    {
        (void)_fLock.acquire_write();

        _fIO.set_handle(_fLock.get_handle());

        size = ACE_OS::strlen(sectionName) + ACE_OS::strlen(keyName) + ACE_OS::strlen(keyValue) + 9; /*���������ַ�10 �������ַ� 13 ��'[',']','='*/


        /* �������ļ�ĩβ��Ӳ�������. */

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
        /* ������sectionʱ. */
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

            /* ����Ͳ���sectionNameһ��,�����key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                innerParam.bSectionMatched = true;
                while (innerParam.bContinue && (size_t)i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* �û�������section������key,��������ѭ��. */
                    {
                        innerParam.index1 = i;
                        innerParam.bContinue = false;
                        break;
                    }
                    if (CheckCharIsEnterOrNewLine(c)|| CheckCharIsSpaceOrTab(c)) /* �س� - 13,  ���� - 10, �ո� - 32, Tab�� - 9 */
                    {
                        continue;
                    }
                    /* ����ע�ͣ����ҵ����з�. */
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
                        /* ����Key��. */
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

                        /* ��key���������keyName����ƥ��. */
                        if (bKeyFinded)
                        {
                            (void)trim(temp);
                            if (ACE_OS::strcmp(temp,keyName) == 0) /* ƥ��������. */
                            {
                                innerParam.bKeyMatched = true;
                                j = 0;
                                bool bKeyValueFinded = false;
                                while (!bKeyValueFinded && (size_t)i < _bufferSize)
                                {
                                    c = *(_buffer + i++);
                                    if (CheckCharIsEnterOrNewLine(c) || c == CONFIG_NOTE_SYMBOL) /* ��Key��ֵ�ĵ�һ���ַ�Ϊ "�س�","����","ע��" �����. */
                                    {
                                        innerParam.index2 = i - 2;
                                        innerParam.bContinue = false;
                                        break;
                                    }
                                    else if (CheckCharIsSpaceOrTab(c)) /* Ϊ"�ո�"��"Tab��"�����. */
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        /* ���������,��ȡkeyֵ. */
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
                            else    /* ��ƥ��������,�ƶ������. */
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
        /* ��������ע����ʱ(��CONFIG_NOTE_SYMBOL��ͷ). */
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
 *    ����        : del
 *    ����        : ɾ������,ɾ��ָ��section��������key
 *    ����        : 2004-4-16 by wzg@szhtp.com
 *    ����        :
 *    �������    : sectionName
 *    �������    : ��
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
 *****************************************************************
 */

int32_t CAC_Config::del(const char *sectionName)
{
    /* ��ȡ����,�����ļ�,��ȡ����. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* �ͷ���. */
    (void)_fLock.release();

    /************************************************************************/
    /*                          ������������                                */
    /************************************************************************/

    int32_t i = 0;//, k = 0;
    int32_t index1 = 0, index2 = 0;
    char    c;
    bool    bContinue        = true;
    bool    bSectionMatched = false;

    while(bContinue && (size_t)i < _bufferSize)
    {
        /* ������sectionʱ. */
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

                /* ����Ͳ���sectionNameһ��,�����key. */
                (void)trim(temp);
                if (ACE_OS::strcmp(temp,sectionName) == 0)
                {
                    index1 = i - 1;
                    bSectionMatched = true;

                }/* end of if(section matched). */
            }
        }
        /* ��������ע����ʱ(��CONFIG_NOTE_SYMBOL��ͷ). */
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
    /*                        �������������ļ�                                */
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

    /* ȥ��index1��index2֮�����������.��ǰ��ͺ�������ݼӵ������ļ���. */

    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    /* ��������ļ�. */
    (void)_fIO.truncate(0);

    /* ���͵�һ�����ݿ�(������). */
    (void)_fIO.seek(0,SEEK_SET);

    size_t size = (size_t)index1;
    char *tempBuf = new char[size];
    (void)ACE_OS::strncpy(tempBuf,_buffer,size);
    (void)_fIO.send(tempBuf,size);
    delete [] tempBuf;
    tempBuf = 0;

    /* ���͵ڶ������ݿ�(������). */

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
 *    ����        : del
 *    ����        : ɾ������,ɾ��ָ��section����ָ����key
 *    ����        : 2004-4-16 by wzg@szhtp.com
 *    ����        :
 *    �������    : sectionName
 *                  keyName
 *    �������    : ��
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
 *****************************************************************
 */
int32_t CAC_Config::del(const char *sectionName, const char *keyName)
{
    /* ��ȡ����,�����ļ�,��ȡ����. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* �ͷ���. */
    (void)_fLock.release();

    /************************************************************************/
    /*                            ������������                                */
    /************************************************************************/

    int32_t i = 0; //, k = 0;
    int32_t index2 = 0;
    char    c;
    bool    bContinue        = true;
    bool    bSectionMatched = false;
    bool    bKeyMatched        = false;

    while(bContinue && (size_t)i < _bufferSize)
    {
        /* ������sectionʱ. */
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

            /* ����Ͳ���sectionNameһ��,�����key. */
            if (ACE_OS::strcmp(temp,sectionName) == 0)
            {
                bSectionMatched = true;
                while (bContinue && (size_t)i< _bufferSize)
                {
                    c = *(_buffer+i++);
                    if (c == '[' || c == '=') /* �û�������section������key,��������ѭ��. */
                    {
                        bContinue = false;
                        break;
                    }
                    if (CheckCharIsEnterOrNewLine(c)|| CheckCharIsSpaceOrTab(c)) /* �س� - 13,  ���� - 10, �ո� - 32, Tab�� - 9 */
                    {
                        continue;
                    }
                    /* ����ע�ͣ����ҵ����з�. */
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
                        /* ����Key��. */
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

                        /* ��key���������keyName����ƥ��. */
                        if (bKeyFinded)
                        {
                            if (ACE_OS::strcmp(temp,keyName) == 0) /* ƥ��������. */
                            {
                                bKeyMatched = true;
                                index2 = i;
                                bContinue = false;

                            }
                            else    /* ��ƥ��������,�ƶ������. */
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
        /* ��������ע����ʱ(��CONFIG_NOTE_SYMBOL��ͷ). */
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

    /* �ҵ�ָ��key����ͷ. */
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

    /* �ҵ�ָ��key����β. */
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
    /*                        �������������ļ�                                */
    /************************************************************************/

    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    /* ��������ļ�. */
    (void)_fIO.truncate(0);

    /* ���͵�һ�����ݿ�(������). */
    (void)_fIO.seek(0,SEEK_SET);

    size_t size = (size_t)headIndex;
    char *tempBuf = new char[size];
    (void)ACE_OS::strncpy(tempBuf,_buffer,size);
    (void)_fIO.send(tempBuf,size);
    delete [] tempBuf;
    tempBuf = 0;

    /* ���͵ڶ������ݿ�(������). */

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
 *    ����        : recv
 *    ����        : ��ȡ����,����_buffer��.
 *    ����        : 2004-4-16 by wzg@szhtp.com
 *    ����        :
 *    �������    : ��
 *    �������    : ��
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
 *****************************************************************
 */
int32_t CAC_Config::recv()
{
    /* ��ȡ�����ļ���Ϣ. */
    ACE_FILE_Info fInfo;

    if (_fIO.get_info (fInfo) == -1)
    {
        return -1;
    }

    if (fInfo.size_ <= 0)
    {
        return -1;
    }

    _bufferSize = (size_t)fInfo.size_;  /*��Ϣ����*/

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
 *    ����        : free
 *    ����        : �ͷ��ڴ�
 *    ����        : 2004-4-22 by wzg@szhtp.com
 *    ����        :
 *    �������    : ��
 *    �������    : ��
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
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
 *    ����        : backup
 *    ����        : ���������ļ�
 *    ����        : 2004-4-22 by wzg@szhtp.com
 *    ����        :
 *    �������    : backupFileName    - �����ļ���
 *    �������    : ��
 *    ����ֵ        : �ɹ� - 0
 *                ʧ�� - -1
 *****************************************************************
 */
int32_t CAC_Config::backup(const char *backupFileName)
{
    /* �������ļ�����ȡ����. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    (void)_fLock.release();

    /* ���������ļ�,��������д��. */
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
    *�ж������Ƿ�Ϸ�
    */

    if (ACE_OS::strcmp(old_section,"") == 0)
    {
        return -1;
    }

    if (ACE_OS::strcmp(new_section,"") == 0)
    {
        return -1;
    }


    /* ��ȡ����,�����ļ�,��ȡ����. */
    (void)_fLock.acquire_read();

    _fIO.set_handle(_fLock.get_handle());

    (void)recv();

    /* �ͷ���. */
    (void)_fLock.release();
    //_fLock.remove(0);

    /************************************************************************/
    /*                            ������������                                */
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
        /* ������sectionʱ. */
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

            /* ����Ͳ���sectionNameһ��,�����key. */
            if (ACE_OS::strcmp(temp,old_section) == 0)
            {
                bSectionMatched = true;
                bContinue        = false;

            }/* end of if(section matched). */

        }
        /* ��������ע����ʱ(��GE_CONFIG_NOTE_SYMBOL��ͷ). */
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
    /*                        ������ʱ�����ļ�                                */
    /************************************************************************/


    /*
    *1. ���û�������key����ʱ.
    *
    *����index����¼keyֵ�ĳ��ȼ�λ��.
    *Ȼ��_buffer������д�뵽��ʱ�ļ��У����޸�keyֵ.
    */


    if (bSectionMatched)
    {
        ret = 0;
        size_t    size;
        char    *tempBuf;

        (void)_fLock.acquire_write();

        _fIO.set_handle(_fLock.get_handle());

        /* ��������ļ�. */
        (void)_fIO.truncate(0);

        /* ���͵�һ�����ݿ�(������). */
        (void)_fIO.seek(0,SEEK_SET);
        size = (size_t)index1;
        tempBuf = new char[size];
        (void)ACE_OS::strncpy(tempBuf,_buffer, size);
        (void)_fIO.send(tempBuf,size);
        delete [] tempBuf;
        tempBuf = 0;

        /* ���͵ڶ������ݿ�(������). */
        size = ACE_OS::strlen(new_section);
        char    t[100];
        (void)ACE_OS::sprintf(t,"%s",new_section);
        (void)_fIO.send(t,size);

        /* ���͵��������ݿ�(������). */
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

    /* ��ȡ����,�����ļ�,��ȡ����. */
    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    (void)_fIO.seek(0,SEEK_END);

    size_t    size = ACE_OS::strlen(strLog);
    char    *buf;
    buf = new char[size + 2];

    (void)ACE_OS::sprintf(buf,"%s%c%c",strLog,13,10);

    (void)_fIO.send(buf,size+2);

    delete [] buf;

    /* �ͷ���. */
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

    /* ��ȡ����,�����ļ�,��ȡ����. */
    (void)_fLock.acquire_write();

    _fIO.set_handle(_fLock.get_handle());

    (void)_fIO.seek(0,SEEK_END);

    size_t    size = ACE_OS::strlen(strLog);
    char    *buf;
    buf = new char[size + 2];

    (void)ACE_OS::sprintf(buf,"%s%c%c",strLog,13,10);

    (void)_fIO.send(buf,size+2);

    delete [] buf;

    /* �ͷ���. */
    (void)_fLock.release();

    return 0;
}



