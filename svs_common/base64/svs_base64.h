/******************************************************
  Copyright (C), 2009-2019, AllCam Tech. Co., Ltd.
  File name:  SVSBase64.h
  Version:  1.1
  Date:  2009-02-03
  Description:  ����SVSBase64�࣬ʵ�ֶԶ��������ݵ�SVSBase64ת��ͽ���
  Others:
  Function List:
      1. encode �����������ݽ���SVSBase64����
      2. decode ���ַ�������SVSBase64����
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
      Description:  �����������ݽ���SVSBase64����
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:   pContent ����������
      Output:
      Return:  ����SVSBase64�������ַ���
      Others:
    *******************************************************/
    string encode(Binary *pContent) const;

    string encode(uint32 len, const uint8 *pContent) const;

    /*******************************************************
      Function:  decode
      Description:  ���ַ�������SVSBase64����
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:   pContent �ַ���
      Output:
      Return:  ����SVSBase64�����Ķ���������
      Others:
    *******************************************************/
    Binary *decode(const string &pContent) const;

    int32_t decode(const string &pContent, uint8 *&pDecBuf) const;

private:

    void encodeBuf2(const uint8 *buf, char *buf2, int32_t pad) const;

    int32_t decodeBuf(uint8 *buf, int32_t &pad) const;


};

#endif

