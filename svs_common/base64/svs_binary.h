/******************************************************
  Copyright (C), 2009-2019, AllCam Tech. Co., Ltd.
  File name:  Binary.h
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
      Description:  获取二进制数据基址指针
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:
      Output:
      Return:  返回二进制数据基址指针
      Others:
    *******************************************************/
    uint8 *getBase();

    /*******************************************************
      Function:  getLength
      Description:  获取二进制数据长度
      Calls:
      Called By:
      Table Accessed:
      Table Updated:
      Input:
      Output:
      Return:  返回二进制数据长度
      Others:
    *******************************************************/
    uint32 getLength() const;

    //实现深层拷贝
    int32_t copy(Binary bin);

    //给实例赋值
    int32_t copy(uint32 len, const uint8 *bytes);

private:
    uint32 m_nLen;

    uint8 *m_pDataBuf;
};

#endif

