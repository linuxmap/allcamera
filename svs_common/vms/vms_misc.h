#ifndef _SVS_MISC_H
#define _SVS_MISC_H

#include <time.h>
#include <assert.h>
#include <string.h>

#include <ace/OS.h>
#include <ace/Message_Block.h>
#include <ace/OS_NS_time.h>

#ifndef WIN32
#include <netinet/in.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <vms/vms_adt.h>
#include <vms/vms_df.h>
#include <vms/vms_cm.h>

#pragma pack(push, 1)


inline const char *zone_org_code(const char * const strId, char *strDest, int32_t len)
{
        assert(SVS_ZONE_ORG_LEN + 1 <= len);
        memcpy(strDest, strId, SVS_ZONE_ORG_LEN);
        strDest[SVS_ZONE_ORG_LEN] = '\0';
        return strDest;
}


#define GEIP_RETURN_DATA(pch) (pch + sizeof(ACE_HANDLE) + sizeof(GEIPCOMMONHDR))

struct SVS_RECV_MSG_HDR
{
        union
        {
                ACE_HANDLE handle;  //����SOCKET���
                uint32_t   sendQueue;      //����SOCKET��Ӧ�ķ��Ͷ���
        }hdr;
};

inline const char *GetCommonHdr(const char *pch)
{
        return(pch + sizeof(SVS_RECV_MSG_HDR));
}


inline SVS_HEADER *get_common_hdr(const char * const pch)
{
        return (SVS_HEADER *)(pch + sizeof(SVS_RECV_MSG_HDR));
}


inline SVS_HEADER *get_common_hdr(ACE_Message_Block *mb)
{
        return (SVS_HEADER *)(mb->rd_ptr() + sizeof(SVS_RECV_MSG_HDR));
}

inline const char *GetMessageData(const char *pch)
{
        return GetCommonHdr(pch) + sizeof(SVS_HEADER);
}

inline uint16_t transaction_number(uint16_t transactionno)
{
        return transactionno++;
}

inline void FillCommonHeader(PSVS_HEADER pHdr, uint32_t MsgType, uint16_t TransactionNo, uint32_t PacketLength)
{
        pHdr->ProtocolVersion = SVS_PROTOCOL_VERSION;
        pHdr->MsgType = MsgType;
        pHdr->PacketLength = PacketLength;
        pHdr->TransactionNo = TransactionNo;
}

inline void FillCommonHeader(SVS_HEADER &hdr, uint32_t MsgType, uint16_t TransactionNo, uint32_t PacketLength)
{
        hdr.ProtocolVersion = SVS_PROTOCOL_VERSION;
        hdr.MsgType = MsgType;
        hdr.PacketLength = PacketLength;
        hdr.TransactionNo = TransactionNo;
}

inline void fill_svs_signal_header(SVS_HEADER &hdr, uint16_t version, uint32_t MsgType, uint16_t TransactionNo, uint32_t PacketLength)
{
        hdr.ProtocolVersion = htons(version);
        hdr.MsgType = htons(MsgType);
        hdr.PacketLength = htons(PacketLength);
        hdr.TransactionNo = htons(TransactionNo);
}

inline void fill_svs_signal_header(SVS_HEADER *pHdr, uint16_t version, uint32_t MsgType, uint16_t TransactionNo, uint32_t PacketLength)
{
        fill_svs_signal_header(*pHdr, version, MsgType, TransactionNo, PacketLength);
}

inline void svs_header_byteorder_network2host(SVS_HEADER &hdr)
{
        hdr.ProtocolVersion = ntohs(hdr.ProtocolVersion);
        hdr.MsgType = ntohs(hdr.MsgType);
        hdr.PacketLength = ntohs(hdr.PacketLength);
        hdr.TransactionNo = ntohs(hdr.TransactionNo);
}

//�����λ����Ϊ1
inline uint16_t transactionno_respond(uint16_t &transactionno)
{
        transactionno |= 0x8000;
        return transactionno;
}

//ȡ���λ
inline uint16_t highestbit_transactionno(uint16_t &transactionno)
{
        return((transactionno & 0x8000) >> 15);
}


inline uint16_t reset_transactionno(uint16_t &transactionno)
{
        transactionno &= 0x7FFF;
        return transactionno;
}

inline bool check_common_header(SVS_HEADER &hdr)
{
        if(SVS_PROTOCOL_VERSION != hdr.ProtocolVersion || sizeof(SVS_HEADER) > hdr.PacketLength)
                return false;
        else
                return true;
}


inline void nhdr2hhdr(SVS_HEADER *pHdr)
{
    pHdr->ProtocolVersion = ntohs(pHdr->ProtocolVersion);
    pHdr->PacketLength = ntohl(pHdr->PacketLength);
    pHdr->MsgType = ntohl(pHdr->MsgType);
    pHdr->TransactionNo = ntohs(pHdr->TransactionNo);
}


inline void add_timestamp(char *buf)
{
        assert(NULL != buf);

        ACE_Time_Value spend = ACE_OS::gettimeofday();
        struct timeval *ptv = (struct timeval *)buf;
        ptv->tv_sec = spend.sec();
        ptv->tv_usec = spend.usec();
}

#pragma pack(pop)

inline ACE_HANDLE byte2handle(const char *pch)
{
        return *((ACE_HANDLE *)pch);
}

inline uint32_t byte2int(ACE_Message_Block *mb)
{
        return ((SVS_RECV_MSG_HDR *)mb->rd_ptr())->hdr.sendQueue;
}

inline void recv_msg_hdr(ACE_Message_Block *mb, uint32_t sendQueue)
{
        ((SVS_RECV_MSG_HDR *)mb->rd_ptr())->hdr.sendQueue = sendQueue;
}

inline char * dumpMsgHead(const SVS_HEADER *pHeader, char * pszMsgHead, uint16_t nSize)
{
    if(NULL == pszMsgHead)
    {
        return NULL;
    }

    ACE_OS::snprintf(pszMsgHead,
                     nSize,
                    " msghead[ ProtocolVersion:0x%04x "
         "MsgType:0x%04x TransactionNo:0x%04x,PacketLength:%d]",
         pHeader->ProtocolVersion, pHeader->MsgType,
         pHeader->TransactionNo, pHeader->PacketLength);

    return pszMsgHead;
}

inline uint64_t
SVS_HTONLL(uint64_t in)
{
    return ((uint64_t)ACE_HTONL((in >> 32) & 0xffffffff) |
        ((uint64_t)ACE_HTONL(in & 0xffffffff) << 32));
}
inline uint64_t
SVS_NTOHLL(uint64_t in)
{
    return ((uint64_t)ACE_NTOHL((in >> 32) & 0xffffffff) |
        ((uint64_t)ACE_NTOHL(in & 0xffffffff) << 32));
}

//������־ʱ�价��
class SVS_UTIL_LIB
{
public:
#ifndef WIN32
    static int32_t init_log_stenv()
    {
        const char *strName = "ACE_LOG_TIMESTAMP";
        const char *strExpectedValue = "DATE";
        const int32_t nValueSize = strlen(strExpectedValue);

        char *pstrValue = getenv(strName);
        if(NULL != pstrValue)
        {
            if(0 == memcmp(strExpectedValue, pstrValue, nValueSize))
                return 0;
        }

        if(-1 == setenv(strName, strExpectedValue, 1))
        {
            return -1;
        }

        return 0;
    }

    static int32_t init_svr_coreenv()
    {
        struct rlimit rlim;
        struct rlimit rlim_new;

        if (getrlimit(RLIMIT_CORE, &rlim)==0)
        {
            rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY; //rlim_new.rlim_max = 0; //RLIM_INFINITY;

            if (setrlimit(RLIMIT_CORE, &rlim_new) !=0)
            {
                // failed. try raising just to the old max
                rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
                (void) setrlimit(RLIMIT_CORE, &rlim_new);
            }
        }

        return 0;
    }

    static int32_t init_svr_nofileenv(int32_t value)
    {
        struct rlimit rlim;
        struct rlimit rlim_new;

        if (getrlimit(RLIMIT_NOFILE, &rlim)==0)
        {
            rlim_new.rlim_cur = rlim_new.rlim_max = value;

            if (setrlimit(RLIMIT_NOFILE, &rlim_new) !=0)
            {
                // failed. try raising just to the old max
                rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
                (void) setrlimit(RLIMIT_NOFILE, &rlim_new);
            }
        }

        return 0;
    }

    static int32_t init_svr_stkenv(int32_t value)
    {
        struct rlimit rlim;
        struct rlimit rlim_new;

        if (getrlimit(RLIMIT_STACK, &rlim)==0)
        {
            rlim_new.rlim_cur = rlim_new.rlim_max = value;

            if (setrlimit(RLIMIT_STACK, &rlim_new) !=0)
            {
                // failed. try raising just to the old max
                rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
                (void) setrlimit(RLIMIT_STACK, &rlim_new);
            }
        }

        return 0;
    }

#endif
};

#endif //_SVS_MISC_H

