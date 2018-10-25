/******************************************************
  Copyright (C), 2009-2019, AllCam Tech. Co., Ltd.
  File name:  Binary.cpp
  Version:  1.1
  Date:  2009-02-03
  Description:  定义Binary类，用于保存二进制数据
  Others:
  Function List:
      1. getBase 获取二进制数据基址指针
      2. getLength 获取二进制数据长度
  History:
      1.  Date:
          Author:
          Modification:
******************************************************/
#include "svs_binary.h"
#include "stdlib.h"
#include "string.h"
//#include <assert.h>

Binary::Binary()
{
    m_nLen = 0;
    m_pDataBuf = NULL;
}

Binary::~Binary()
{
    //SAFE_DELETE_PTR(m_pDataBuf)
    delete[] m_pDataBuf;
    m_pDataBuf = NULL;
    m_nLen = 0;
}

uint8 *Binary::getBase()
{
    return m_pDataBuf;
}

uint32 Binary::getLength() const
{
    return m_nLen;
}

int32_t Binary::copy(Binary bin)
{
    if (bin.getBase() == NULL && bin.getLength() == 0)
    {
//        SAFE_DELETE_PTR(m_pDataBuf);
        if (NULL != m_pDataBuf)
        {
            delete[] m_pDataBuf;
            m_pDataBuf = NULL;
        }

        m_nLen = 0;

        return 0;
    }

    //数据非法
    if (bin.getBase() == NULL || bin.getLength() == 0)
    {
        return -1;
    }

    if (m_pDataBuf != NULL)
    {
        delete m_pDataBuf;
        m_pDataBuf = NULL;
    }

    m_nLen = bin.getLength();

    try
    {
        m_pDataBuf = new uint8[bin.getLength() + 1];
    }
    catch (...)
    {
        delete[] m_pDataBuf;
        m_pDataBuf = NULL;

        return -1;
    }

    //1055
    //746*/
//    (void)assert(m_pDataBuf != NULL);
    //e746
    //1055

    if (NULL != m_pDataBuf)
    {
        uint8 *pBase = bin.getBase();
        if (NULL != pBase)
        {
            (void)memcpy(m_pDataBuf, pBase, m_nLen);
        }
        m_pDataBuf[m_nLen] = 0;
    }


    return 0;
}

int32_t Binary::copy(uint32 len, const uint8 *bytes)
{

    if (len == 0 && bytes == NULL)
    {
        if (NULL != m_pDataBuf)
        {
            delete[] m_pDataBuf;
            m_pDataBuf = NULL;
        }

        return -1;
    }

    if (bytes == NULL)
    {
        return -1;
    }

    if (m_pDataBuf != NULL)
    {

        delete[] m_pDataBuf;
        m_pDataBuf = NULL;

    }

    m_nLen = len;

    try
    {
        m_pDataBuf = new uint8[m_nLen + 1];//(uint8 *)malloc(m_nLen + 1);
    }
    catch (...)
    {
        delete[] m_pDataBuf;
        m_pDataBuf = NULL;
        return -1;
    }

//    (void)assert(m_pDataBuf != NULL);   730

    if (NULL != m_pDataBuf)
    {
        memcpy(m_pDataBuf, bytes, m_nLen);
        m_pDataBuf[m_nLen] = 0;
    }

    return 0;
}

