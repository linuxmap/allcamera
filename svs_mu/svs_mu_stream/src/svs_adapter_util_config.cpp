/*
 * =====================================================================================
 *
 *       Filename:  CSVS_Config.cpp
 *
 *    Description:  閰嶇疆鏂囦欢瑙ｆ瀽绫汇�
 *
 *        Version:  1.0
 *        Created:  01/05/2011 07:16:37 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Justin Lei GAO (leikao), leikao@gmail.com
 *        Company:
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "svs_adapter_util_config.h"

using namespace std;

CSVS_Config::CSVS_Config(const char* iniFilename)
{
    m_strIniFilename = (char*)iniFilename;
    m_secList.clear();

    (void)init();
}


CSVS_Config::~CSVS_Config()
{
    try
    {
        m_strIniFilename.erase();
    }
    catch(...)
    {}
}


/* Read the .ini file */
int32_t CSVS_Config::init()
{
    struct stat sb;
    if (stat(m_strIniFilename.c_str(), &sb) == -1)
    {
        perror("stat");
        return EXIT_FAILURE;
    }

    FILE* fd = fopen(m_strIniFilename.c_str(), "r");
    if(NULL == fd)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    char* pszSection = (char*)malloc(MAX_ITEM_NAME_LENGTH + 1); /* Section */
    if(NULL == pszSection)
    {
        perror("CSVS_Config::init");
        return EXIT_FAILURE;
    }
    memset(pszSection, 0x0, MAX_ITEM_NAME_LENGTH + 1);

    char* pszKey = (char*)malloc(MAX_ITEM_NAME_LENGTH + 1); /* key */
    if(NULL == pszKey)
    {
        free(pszSection);
        perror("CSVS_Config::init");
        return EXIT_FAILURE;
    }
    memset(pszKey, 0x0, MAX_ITEM_NAME_LENGTH + 1);
    std::string strKey;

    char* pszValue = (char*)malloc(MAX_LINE_SIZE + 1); /* Value */
    if(NULL == pszValue)
    {
        free(pszSection);
        free(pszKey);
        perror("CSVS_Config::init");
        return EXIT_FAILURE;
    }
    memset(pszValue, 0x0, MAX_LINE_SIZE + 1);
    std::string strValue;

    char* pszLine = (char*)malloc(MAX_LINE_SIZE + 1); /* Line */
    if(NULL == pszLine)
    {
        free(pszSection);
        free(pszKey);
        free(pszValue);
        perror("CSVS_Config::init");
        return EXIT_FAILURE;
    }
    memset(pszLine, 0x0, MAX_LINE_SIZE + 1);

    COMMENT_LIST  commentList;
    KEYVALUE_ITEM kvItem;
    SECTION_ITEM  secItem;
    while(NULL != fgets(pszLine, MAX_LINE_SIZE, fd))
    {
        switch(pszLine[0])
        {
            case '[':                           /* Section */
                {
                    memset(pszSection, 0x0, MAX_ITEM_NAME_LENGTH + 1);
                    int32_t rc = sscanf(pszLine, "[%[^]]s", pszSection);
                    if(rc > 0)
                    {
                        // 这里完成SecItem的初始化
                        secItem.Section.erase();
                        secItem.Section.append(pszSection);
                        secItem.ItemComment = commentList;
                        secItem.KvList.clear();
                        m_secList.push_back(secItem);

                        commentList.clear();
                    }
                }
                break;
            case '#':                           /* Comment */
                {
                    commentList.push_back(pszLine);
                }
                break;
            case '\n':
            case '\r':                          /* Balank line */
                {
                    commentList.push_back("\n");
                }
                break;
            default:                           /* k-v pair */
                {
                    memset(pszKey, 0x0, MAX_ITEM_NAME_LENGTH + 1);
                    memset(pszValue, 0x0, MAX_LINE_SIZE + 1);
                    int32_t rc = sscanf(pszLine, "%[^=]=%[^=]", pszKey, pszValue);
                    if(rc > 0)
                    {
                        strKey.erase();
                        strKey.append(pszKey);
                        strValue.erase();
                        strValue.append(rtrim(pszValue));

                        // 直接添加到最后一个Section即可
                        if(!m_secList.empty())
                        {
                            SECTION_ITEM &curIterm = m_secList.back();
                            kvItem.ItemComment = commentList;
                            kvItem.Key         = strKey;
                            kvItem.Value       = strValue;
                            curIterm.KvList.push_back(kvItem);

                            commentList.clear();
                        }
                    }
                }
                break;
        }
    }

    fclose(fd);

    free(pszSection);
    free(pszKey);
    free(pszValue);
    free(pszLine);
    return EXIT_SUCCESS;
}

int32_t CSVS_Config::get(const char* sectionName,
                    const char* itemName,
                    char* keyValue)
{
    string strSection((char*)sectionName);
    string strItem((char*)itemName);

    SECTION_LIST::iterator iter = m_secList.begin();
    for (; iter != m_secList.end(); iter++)
    {
        if (iter->Section == strSection)
        {
            break;
        }
    }
    if (m_secList.end() == iter)
    {
        return EXIT_FAILURE;
    }

    KV_LIST::iterator kvIter = iter->KvList.begin();
    for (; kvIter != iter->KvList.end(); kvIter++)
    {
        if (kvIter->Key == strItem)
        {
            break;
        }
    }
    if (iter->KvList.end() == kvIter)
    {
        return EXIT_FAILURE;
    }

    strcpy(keyValue, kvIter->Value.c_str());
    return EXIT_SUCCESS;
}

int32_t CSVS_Config::get(const char* sectionName,
                    const char* itemName,
                    char* keyValue,
                    uint32_t unValueLength)
{
    string strSection((char*) sectionName);
    string strItem((char*) itemName);
    SECTION_LIST::iterator secIter = m_secList.begin();
    for (; secIter != m_secList.end(); secIter++)
    {
        if (secIter->Section == strSection)
        {
            break;
        }
    }
    if (m_secList.end() == secIter)
    {
        return EXIT_FAILURE;
    }

    KV_LIST::iterator kvIter = secIter->KvList.begin();
    for (; kvIter != secIter->KvList.end(); kvIter++)
    {
        if (kvIter->Key == strItem)
        {
            break;
        }
    }
    if (secIter->KvList.end() == kvIter)
    {
        return EXIT_FAILURE;
    }

    strncpy(keyValue, kvIter->Value.c_str(), unValueLength);
    return EXIT_SUCCESS;
}


int32_t CSVS_Config::set(const char* sectionName, const char* itemName, const char* keyValue)
{
    string strSection((char*) sectionName);
    string strKey((char*) itemName);
    string strValue((char*) keyValue);
    SECTION_LIST::iterator iter = m_secList.begin();
    for (; iter != m_secList.end(); iter++)
    {
        if (iter->Section == strSection)
        {
            break;
        }
    }
    if (m_secList.end() == iter)
    {
        return EXIT_FAILURE;
    }

    KV_LIST::iterator kvIter = iter->KvList.begin();
    for (; kvIter != iter->KvList.end(); kvIter++)
    {
        if (kvIter->Key == strKey)
        {
            break;
        }
    }
    if (iter->KvList.end() == kvIter)
    {
        return EXIT_FAILURE;
    }

    std::string strOldValue = kvIter->Value;
    kvIter->Value = strValue;

    // 保存文件
    if (EXIT_SUCCESS != save())
    {
        // 保存文件失败，需要把配置值还原
        kvIter->Value = strOldValue;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
void CSVS_Config::dump(FILE *pFile)
{
    for (SECTION_LIST::iterator iter = m_secList.begin(); iter != m_secList.end(); iter++)
    {
        SECTION_ITEM &secItem = *iter;
        // 打印注释
        for (COMMENT_LIST::iterator listIter = secItem.ItemComment.begin();
                listIter != secItem.ItemComment.end(); listIter++)
        {
            fprintf(pFile, "%s", listIter->c_str());
        }

        // 打印Section
        fprintf(pFile, "[%s]\n", secItem.Section.c_str());

        // 打印K-V配置
        KV_LIST::iterator kviter = secItem.KvList.begin();
        for(; kviter != secItem.KvList.end(); ++kviter)
        {
            // 先打印注释
            for (COMMENT_LIST::iterator listIter = kviter->ItemComment.begin();
                            listIter != kviter->ItemComment.end(); listIter++)
            {
                fprintf(pFile, "%s", listIter->c_str());
            }

            // 打印KeyValue
            fprintf(pFile, "%s=%s\n",
                    kviter->Key.c_str(), kviter->Value.c_str());
        }
    }

    return;
}

char* CSVS_Config::rtrim(const char* ostr) const
{
    char* pHeader = (char*)ostr;

    /* Head */
    while('\r' == (*pHeader) || '\n' == (*pHeader) || ' ' == (*pHeader) || '\t' == (*pHeader))
    {
        ++pHeader;
    }

    /* Tail */
    int32_t len = (int32_t)strlen(pHeader);
    while(0 < len)
    {
        if('\r' == (pHeader[len-1]) || '\n' == (pHeader[len-1]) || ' ' == (pHeader[len-1]) || '\t' == (pHeader[len-1]))
        {
            --len;
        }
        else
        {
            break;
        }
    }

    pHeader[len] = '\0';
    return pHeader;
}

int32_t CSVS_Config::save()
{
    // 先保存原来的配置文件
    string strBackFile = m_strIniFilename + INI_BACK_POSTFIX;

    // ��������ļ��Ѿ�����ʱ��rename() ��ʧ�ܵ����⡣
    (void)remove(strBackFile.c_str());

    if (0 != rename(m_strIniFilename.c_str(), strBackFile.c_str()))
    {
        return EXIT_FAILURE;
    }

    // 打开原有文件
    FILE* fd = fopen(m_strIniFilename.c_str(), "w+");
    if (NULL == fd)
    {
        perror("save::fopen");
        // 配置文件还原
        (void)rename(strBackFile.c_str(), m_strIniFilename.c_str());
        return EXIT_FAILURE;
    }

    // 写文件
    dump(fd);

    fclose(fd);
    (void)remove(strBackFile.c_str());

    return EXIT_SUCCESS;
}

#ifdef CONFIG_TEST
int32_t main()
{
    CSVS_Config config("./svs_mu_record.conf");
    config.dump();

    config.set("MEDIA_SERVICE", "MediaIp", "10.137.11.11111111");
    return 0;
}
#endif


