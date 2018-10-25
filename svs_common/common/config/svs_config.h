// CAC_Config.h: interface for the CAC_Config class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAC_CONFIG_H__045D227C_0030_4C6F_8464_1334CF7B6A61__INCLUDED_)
#define AFX_CAC_CONFIG_H__045D227C_0030_4C6F_8464_1334CF7B6A61__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ace/File_Lock.h"
#include "ace/FILE_Connector.h"
#include "ace/Vector_T.h"
#include "ace/CDR_Stream.h"
#include "ace/Auto_Ptr.h"
#include "ace/ACE.h"
#include "ace/OS.h"

//#include "GE_Svr_Header.h"

/*
 * *       Config键值列表结构体定义.
 * */
typedef  struct
_GE_CONFIG_KEY_LIST
{
        char    KeyName[50];
        char    KeyValue[512];

}GE_CONFIG_KEY_LIST, *PGE_CONFIG_KEY_LIST;


/************************************************************************/
/*                                参数定义                                */
/************************************************************************/

#define    CONFIG_NOTE_SYMBOL        ';'
#define CONFIG_FILE_NAME            "config.ini"

#define MAX_CFG_ITEM_LEN         128

class CAC_Config
{
public:
    CAC_Config(const char *fileName = CONFIG_FILE_NAME);
    virtual ~CAC_Config();

    int32_t add_record_log(const char *log, ...);
    int32_t add_log(const char *log, ...);
    int32_t set_section(const char *old_section, const char *new_section);

    /* 备份配置文件 */
    int32_t backup(const char * backupFileName);

    /* 删除配置参数. */
    int32_t del(const char *sectionName, const char *keyName);
    int32_t del(const char *sectionName);

    /* 设置配置参数,如果不存在,则添加. */
    int32_t set(const char *sectionName, const char *keyName, const char *keyValueFormat,...);

    /* 获取配置参数. */
    int32_t get(const char *sectionName, ACE_Vector<GE_CONFIG_KEY_LIST> *vect, uint32_t unLen = MAX_CFG_ITEM_LEN);
    int32_t get(const char *sectionName, const char *keyName, char *outKeyValue, bool bRtrim = true, uint32_t unLen = MAX_CFG_ITEM_LEN);

protected:
    /**
     * 去掉右边第1个空格或者注释(#以后的字符)
     * Stone Shi
     * 2006-6-21 11:13
     */
    void rtrim(char *str)const;


    typedef struct _FUNC_SET_INNER__PARAM
    {
        size_t      index1;
        size_t      index2;
        bool        bContinue;
        bool        bSectionMatched;
        bool        bKeyMatched;
    }FUNC_SET_INNER__PARAM;
    void inner_subFuncOfSet(const char *sectionName,const char *keyName, FUNC_SET_INNER__PARAM & innerParam);

    void inner_subFuncOfGetKey(const char *sectionName, const char *keyName, char *outKeyValue, uint32_t unLen, int32_t & ret);


private:
    int32_t free();
    int32_t recv();
    char * trim(char *pstr)const;
    ACE_File_Lock    _fLock;
    ACE_File_Lock    _fLockTemp;
    ACE_FILE_IO        _fIO;
    ACE_FILE_IO        _fIOTemp;
    const char*         _fName;
    char*            _fNameTemp;
    bool            _bLockReleased;
    bool            _bFileClosed;
    size_t            _bufferSize;
    char*            _buffer;
};

#endif // !defined(AFX_CAC_CONFIG_H__045D227C_0030_4C6F_8464_1334CF7B6A61__INCLUDED_)




