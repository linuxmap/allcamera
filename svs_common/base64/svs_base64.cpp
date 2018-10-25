/******************************************************
  Copyright (C), 2009-2019, AllCam Tech. Co., Ltd.
  File name:  SVSBase64.cpp
  Version:  1.1
  Date:  2009-02-03
  Description:  定义SVSBase64类，实现对二进制数据的SVSBase64转码和解码
  Others:
  Function List:
      1. encode 将二进制数据进行SVSBase64编码
      2. decode 将字符串进行SVSBase64解码
  History:
      1.  Date:
          Author:
          Modification:
******************************************************/
#include "svs_base64.h"
#include <memory>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_DEC_BUF_LEN (1024 * 1024)

// 参数的有效性、合法性均由调用函数负责
void SVSBase64::encodeBuf2(const uint8 *buf, char *buf2, int32_t pad) const
{
    if (NULL == buf || NULL == buf2)
    {
        return ;
    }

    buf2[0] = buf[0] >> 2;
    buf2[1] = (buf[0] & 0x03) << 4 | buf[1] >> 4;
    buf2[2] = (buf[1] & 0x0f) << 2 | buf[2] >> 6;
    buf2[3] = buf[2] & 0x3f;

    int32_t i = 0;
    for (i = 0; i < 4; i++)
    {
        if (buf2[i] >= 0 && buf2[i] <= 25)
        {
            buf2[i] = buf2[i] + 'A';
        }
        else if(buf2[i] >= 26 && buf2[i] <= 51)
        {
            buf2[i] = (buf2[i] - 26) + 'a';
        }
        else if(buf2[i] >= 52 && buf2[i] <= 61)
        {
            buf2[i] = (buf2[i] - 52) + '0';
        }
        else if(buf2[i] == 62)
        {
            buf2[i] = '+';
        }
        else if(buf2[i] == 63)
        {
            buf2[i] = '/';
        }
    }
    if(pad)
    {
        for (i = 4 - pad; i < 4; i++)
        {
            buf2[i] = '=';
        }
    }

}

string SVSBase64::encode(Binary *pContent) const
{
    if (pContent == NULL || pContent->getBase() == NULL || pContent->getLength() == 0)
    {
        return NULL;
    }

    int32_t pad = 0;
    uint32 len = 0;
    uint32 i = 0;

    uint8 buf[4];

    char buf2[5];

    buf2[4] = 0;

    uint8 *pBase = pContent->getBase();

    uint32 iEncBit = 0;

    uint32 iEncTotal = pContent->getLength();

    string strEncBuf;

    while (iEncBit < iEncTotal)
    {
        if ((iEncBit + 3) < iEncTotal)
        {
            len = 3;
            memcpy(buf, pBase, len);
        }
        else
        {
            len = iEncTotal - iEncBit;
            memcpy(buf, pBase, len);
        }

        iEncBit += len;
        pBase += len;

        if (len < 3)
        {
            pad = 3 - (int32_t)len;
            for (i = len; i < 3; i++)
            {
                buf[i]=0;
            }
        }

        encodeBuf2(buf, buf2, pad);

        strEncBuf += (char *)buf2;
    }

    return strEncBuf;
}

string SVSBase64::encode(uint32 len, const uint8 *pContent) const
{
    if (len == 0 || pContent == NULL)
    {
        return NULL;
    }

    Binary *pBin = new Binary();
    (void)pBin->copy(len, pContent);
    string strRet = encode(pBin);
    delete pBin;
    pBin = NULL;

    return strRet;
}

// 参数的有效性、合法性都在函数外部验证
// 成功返回0， 失败返回-1
int32_t SVSBase64::decodeBuf(uint8 *buf, int32_t &pad) const
{
    if (NULL == buf)
    {
        return -1;
    }

    int32_t i = 0;
    for (i = 0; i < 4; i++)
    {
        if (buf[i] >= 'A' && buf[i] <= 'Z')
        {
            buf[i] = buf[i] - 'A';
        }
        else if (buf[i] >= 'a' && buf[i] <= 'z')
        {
            buf[i] = (buf[i] - 'a') + 26;
        }
        else if (buf[i] >= '0' && buf[i] <= '9')
        {
            buf[i] = (buf[i] - '0') + 52;
        }
        else if (buf[i] == '+')
        {
            buf[i] = 62;
        }
        else if (buf[i] == '/')
        {
            buf[i] = 63;
        }
        else if (buf[i] == '=')
        {
            pad = 4 - i;

            for (; i < 4; i++)
            {
                buf[i] = 0;
            }

            break;
        }
        else
        {
            return -1;
        }

    }

    return 0;
}

Binary *SVSBase64::decode(const string &content) const
{
    if (content.length() == 0)
    {
        return NULL;
    }

    int32_t i, pad = 0;
    uint8 buf[4];

    uint32 iDecSrcBit = 0;
    char *pDecSrcBase = (char *)content.c_str();
    uint32 iDecSrcTotal = content.length();
    uint8 decBuf[MAX_DEC_BUF_LEN];
    uint8 *pDecDstBase = decBuf;
    uint32 iDecLen = 0;

    while (iDecSrcBit < iDecSrcTotal)
    {
        i = 0;

        while (i < 4)
        {
            if (iDecSrcBit >= iDecSrcTotal)
            {
                break;
            }

            buf[i] = (uint8)pDecSrcBase[iDecSrcBit++];

            if ((buf[i] != '\r') && (buf[i] != '\n'))
            {
                i++;
            }
        }

        if (i == 0)
        {
            break;
        }

        /*所有进行SVSBase64编码后的数据必须是4的倍数*/
        if (i != 4)
        {
            return NULL;
        }

        int32_t iRet = decodeBuf(buf, pad);
        if (-1 == iRet)
        {
            return NULL;
        }

        pDecDstBase[0] = (char)(buf[0] << 2 | buf[1] >> 4);

        pDecDstBase[1] = (char)(buf[1] << 4 | buf[2] >> 2);

        pDecDstBase[2] = char(buf[2] << 6 | buf[3]);

        iDecLen += (uint32)(3-pad);
        pDecDstBase += 3-pad;
    }

    Binary *pBinContent = new Binary();
    (void)pBinContent->copy(iDecLen, decBuf);

    //printf("iDecLen=%d\n", iDecLen);

    return pBinContent;
}

int32_t SVSBase64::decode(const string &pContent, uint8 *&pDecBuf) const
{
    Binary *pBin = NULL;
    int32_t iLen = -1;

    pBin = decode(pContent);
    if (pBin == NULL)
    {
        return iLen;
    }

    pDecBuf = (uint8 *)malloc(pBin->getLength());
    if (NULL != pDecBuf)
    {
        iLen = (int32_t)pBin->getLength();
        memcpy(pDecBuf, pBin->getBase(), pBin->getLength());
    }

    return iLen;
}

