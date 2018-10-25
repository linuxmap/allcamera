#ifndef HEADER_MD5_CAG_H
#define HEADER_MD5_CAG_H
#ifdef  __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#include "md5.h"

// ��������ܳ�16�ֽڵ�����
unsigned char *EncryptPass16(const char *pass, unsigned char *output);

// ��������ܳ�һ��32λ�ַ���(ÿ�����ֱ������ʮ��������)
char *EncryptPass32(const char *pass, char *output);

#ifdef  __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
