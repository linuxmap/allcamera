/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : StreamUtility.cpp
  �� �� ��   : ����
  ��������   : 2011��7��5��
  ����޸�   :
  ��������   : ���ߺ���
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2011��7��5��
    �޸�����   : �����ļ�

******************************************************************************/
#include "svs_adapter_utility.h"

bool CStreamUtility::less(uint16_t x, uint16_t y)
{
    if (x < y)
    {
        if (y - x > 0x8000)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (x - y > 0x8000)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool CStreamUtility::less(uint32_t x, uint32_t y)
{
    if (x < y)
    {
        if (y - x > 0x80000000)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (x - y > 0x80000000)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}





