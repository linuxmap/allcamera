

/* g++  -g -Wall -o0 -pipe -m32 -fPIC  -D_test_xdm_svsauth_ ../util/algorithm/SvsAuth.cpp ../util/algorithm/md5.c -I../extends/ace_wrappers/ -I../extends/vms/ -I../extends/svs_media/ -I../extends/rtp/ -I../extends/omc/  -I../extends/rtsp//include/rtspcodec -I../util/ -I../util/algorithm -I../util/common -I../util/config -I../util/daemon -I../util/lock -I../util/log -I../util/stat -I../util/thread -I../util/time -I../util/timer  -I../sys/ -I../sys/cache -I../sys/net -I../sys/svs_handle -I../sys/rtp */

#include "string.h"
#include <vms/vms.h>
#include "md5.h"
#include "svs_adapter_vms_auth.h"


void fillRegisterMd5Sum(SVS_MSG_SERVER_REGIST_REQ  * pMsg, const char * szPasswd, uint32_t nPasswdLen)
{
    MD5_CTX Md5Ctx;

    MD5_Init(&Md5Ctx);
    memset(pMsg->Md5CheckSum, 0, sizeof(pMsg->Md5CheckSum));
    MD5_Update(&Md5Ctx, (uint8_t *)pMsg ,  sizeof(SVS_MSG_SERVER_REGIST_REQ));
    MD5_Update(&Md5Ctx, (uint8_t *)szPasswd,  nPasswdLen);
    uint8_t t_HashKey[REGISTER_CHECKSUM_LENGTH];
    memset(t_HashKey, 0 , sizeof(t_HashKey));
    MD5_Final(t_HashKey, &Md5Ctx);

    memcpy(pMsg->Md5CheckSum, t_HashKey, sizeof(t_HashKey));
}



int32_t checkRegisterMd5Sum(SVS_MSG_SERVER_REGIST_REQ  * pMsg, const char * szPasswd, uint32_t nPasswdLen)
{
    MD5_CTX Md5Ctx;
    uint8_t t_HashKeyOrig[REGISTER_CHECKSUM_LENGTH];

    memcpy(t_HashKeyOrig, pMsg->Md5CheckSum, sizeof(t_HashKeyOrig));

    MD5_Init(&Md5Ctx);
    memset(pMsg->Md5CheckSum, 0, sizeof(pMsg->Md5CheckSum));
    MD5_Update(&Md5Ctx, (uint8_t *)pMsg ,  sizeof(SVS_MSG_SERVER_REGIST_REQ));
    MD5_Update(&Md5Ctx, (uint8_t *)szPasswd,  nPasswdLen);
    uint8_t t_HashKeyNew[REGISTER_CHECKSUM_LENGTH];
    memset(t_HashKeyNew, 0 , sizeof(t_HashKeyNew));
    MD5_Final(t_HashKeyNew, &Md5Ctx);

    int32_t ret = memcmp(t_HashKeyOrig, t_HashKeyNew, sizeof(REGISTER_CHECKSUM_LENGTH));
    return (ret != 0) ? -1 : 0;
}

