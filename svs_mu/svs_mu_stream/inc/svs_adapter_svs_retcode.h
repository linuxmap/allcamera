/**
* @file    SvsRetCode.h
* @brief   ϵͳ�����붨��
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-10
*/

#ifndef _SVSRETCODE_H__
#define _SVSRETCODE_H__

// SVSϵͳ�����룬����int����
typedef enum _enSvsRetCode
{
    RET_OK                  = 0x00000000,                          /// �����ɹ�������
    RET_FAIL                = 0x10000000,                          /// ͨ��ʧ�ܷ�����

    RET_ERR_PARAM           = 0x10001001,                          /// ��������
    RET_ERR_UNREGISTED      = 0x10001002,                          /// δע��
    RET_ERR_DISCONNECT      = 0x10001003,                          /// �����ѶϿ�
    RET_ERR_TIMEOUT         = 0x10001004,                          /// ������ʱ
    RET_ERR_LIST_EMPTY      = 0x10001005,                          /// ����Ϊ��
    RET_ERR_STATUS_ABNORMAL = 0x10001006,                          /// ״̬�쳣
    RET_ERR_PARSE_MSG       = 0x10001007,                          /// ������Ϣ����
    RET_ERR_QUEUE_FULL      = 0x10001008,                          /// ������
    RET_ERR_SEND_FAIL       = 0x10001009,                          /// ���ͳ���
    RET_ERR_MEDIA_DATA      = 0x1000100A,                          /// ý�����ݴ���
    RET_ERR_CONFIG_ITEM_NOT_FOUND = 0x1000100B,                    /// ���������

    RET_ERR_SYS             = 0x10002001,                          /// ͨ��ϵͳ����
    RET_ERR_SYS_NEW         = 0x10002002                           /// ϵͳ����newʧ��
} SVS_RET_CODE;

#endif /* _SVSRETCODE_H__ */

