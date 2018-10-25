/* vim: set fenc=utf-8 */
/*
 * =====================================================================================
 *
 *       Filename:  CSVS_Config.h
 *
 *    Description:  实现ini文件的读写
 *
 *        Version:  1.0
 *        Created:  01/05/2011 07:00:43 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Justin Lei GAO (leikao), leikao@gmail.com
 *        Company:
 *
 * =====================================================================================
 */


#ifndef  CSVS_Config_INC
#define  CSVS_Config_INC

#include <string>
#include <list>
#include "svs_log_msg.h"


#define    DEFAULT_INI_FILENAME    "config.ini"    /* 默认配置文件名*/
#define   INI_BACK_POSTFIX        ".cfgbak"          /* 配置文件备份后缀名*/
#define    MAX_ITEM_NAME_LENGTH    128                /* Key或Section最大长度*/
#define    MAX_LINE_SIZE           (8 * 1024)            /* 设定一行不能超过1024字节 */

#define    MAX_CONFIG_VALUE_LENG   256         //  ��������ֵ�ĳ���
#define    MAX_CONFIG_FULL_LENG    356         //  �������õĳ���

typedef std::list<std::string>              COMMENT_LIST;
typedef struct _stKeyValueItem
{
    COMMENT_LIST        ItemComment;
    std::string         Key;
    std::string         Value;
}KEYVALUE_ITEM;

typedef std::list<KEYVALUE_ITEM>     KV_LIST;      /* k-v map */
typedef struct _stSecionItem
{
    COMMENT_LIST        ItemComment;
    std::string         Section;
    KV_LIST             KvList;
}SECTION_ITEM;

typedef std::list<SECTION_ITEM>      SECTION_LIST; /* section-(k-v) map */


enum CONFIG_DEALT_TYPE
{
    STREAM_CONFIG = 0,
    MRU_RECORD_CONFIG,      // ���ں� mbu�ǹ�����һ����
    MRU_STREAM_CONFIG,
    MRU_BACKUP_CONFIG,
    MRU_AG
};

class CSVS_Config
{
public:
    CSVS_Config(const char* iniFilename = DEFAULT_INI_FILENAME);
    ~CSVS_Config();


    /* Get key-value */
    int32_t get(const char* sectionName, const char* itemName, char* keyValue);

    int32_t get(const char* sectionName, const char* itemName, char* keyValue,
            uint32_t unValueLength);

    int32_t set(const char* sectionName, const char* itemName, const char* keyValue);

    void dump(FILE *pFile = stdout);

    int32_t save();

private:
    /* Init */
    int32_t init();

    /* Recursion Trim */
    char* rtrim(const char*) const;
private:
    std::string     m_strIniFilename;
    SECTION_LIST    m_secList;
};

#endif   /* ----- #ifndef CSVS_Config_INC  ----- */
