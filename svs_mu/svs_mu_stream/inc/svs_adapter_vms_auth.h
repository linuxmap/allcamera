#ifndef _SVS_AUTH_H
#define _SVS_AUTH_H

void fillRegisterMd5Sum(SVS_MSG_SERVER_REGIST_REQ  * pMsg, const char * szPasswd, uint32_t nPasswdLen);

int32_t checkRegisterMd5Sum(SVS_MSG_SERVER_REGIST_REQ  * pMsg, const char * szPasswd, uint32_t nPasswdLen);

#endif

