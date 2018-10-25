/******************************************************
  Copyright (C), 2009-2019, AllCam Tech. Co., Ltd.
  File name:  Binary.h
  Version:  1.1
  Date:  2009-02-03
  Description:  ����Binary�࣬���ڱ������������
  Others:
  Function List:
      1. getBase ��ȡ���������ݻ�ַָ��
      2. getLength ��ȡ���������ݳ���
  History:
      1.  Date:
          Author:
          Modification:
******************************************************/
#ifndef __BINARY_H__
#define __BINARY_H__
#include <vms/vms_adt.h>


#ifndef uint8
#define uint8 uint8_t
#endif

#ifndef uint32
#define uint32 uint32_t
#endif

class Binary
{
public:
    Binary();

    ~Binary();

    /*******************************************************
      Function:  getBase
      Description:  ��ȡ���������ݻ�ַָ��
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:
      Output:
      Return:  ���ض��������ݻ�ַָ��
      Others:
    *******************************************************/
    uint8 *getBase();

    /*******************************************************
      Function:  getLength
      Description:  ��ȡ���������ݳ���
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:
      Output:
      Return:  ���ض��������ݳ���
      Others:
    *******************************************************/
    uint32 getLength() const;

    //ʵ����㿽��
    int32_t copy(Binary bin);

    //��ʵ����ֵ
    int32_t copy(uint32 len, const uint8 *bytes);

private:
    uint32 m_nLen;

    uint8 *m_pDataBuf;
};

#endif

