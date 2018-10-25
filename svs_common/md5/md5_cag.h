#ifndef HEADER_MD5_CAG_H
#define HEADER_MD5_CAG_H
#ifdef  __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#include "md5.h"

// 将密码加密成16字节的数串
unsigned char *EncryptPass16(const char *pass, unsigned char *output);

// 将密码加密成一个32位字符串(每个节字变成两个十六进制数)
char *EncryptPass32(const char *pass, char *output);

#ifdef  __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
