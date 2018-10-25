/******************************************************
  Copyright (C), 2009-2019, AllCam Tech. Co., Ltd.
  File name:  SVSBase64.h
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
#ifndef __SVSBase64_H__
#define __SVSBase64_H__

#include <string>
using namespace std;

#include "svs_binary.h"

class SVSBase64
{
public:
    SVSBase64() {}
    ~SVSBase64() {}
    /*******************************************************
      Function:  encode
      Description:  将二进制数据进行SVSBase64编码
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:   pContent 二进制数据
      Output:
      Return:  返回SVSBase64编码后的字符串
      Others:
    *******************************************************/
    string encode(Binary *pContent) const;

    string encode(uint32 len, const uint8 *pContent) const;

    /*******************************************************
      Function:  decode
      Description:  将字符串进行SVSBase64解码
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:   pContent 字符串
      Output:
      Return:  返回SVSBase64解码后的二进制数据
      Others:
    *******************************************************/
    Binary *decode(const string &pContent) const;

    int32_t decode(const string &pContent, uint8 *&pDecBuf) const;

private:

    void encodeBuf2(const uint8 *buf, char *buf2, int32_t pad) const;

    int32_t decodeBuf(uint8 *buf, int32_t &pad) const;


};

#endif

