#ifndef __SVS_RTP_H__
#define __SVS_RTP_H__

#include <bitset>

#pragma pack(push, 1)

#define PROFILE_LEN 2
#define AUTHORIZATION_LEN 16
/*
 *  RTP��չ���ͼ���չ���ݶ�������
 */
enum SVS_RTP_EXTEN_DATA_TYPE
{
    RTP_EXTEN_DATA_TYPE_NORMAL   = 0x0000,  // 0x0000  ����ý����
    RTP_EXTEN_DATA_TYPE_EOS      = 0x0001,  // 0x0001  EOS
    SVS_RTP_EXTEN_TYPE_NAT_REQ   = 0x0101,  // 0x0101  NAT��Խ������Ϣ
    SVS_RTP_EXTEN_TYPE_NAT_RESP  = 0x0102   // 0x0102  NAT��Խ��Ӧ��Ϣ
};


/*
 * ��Ϣ����: rtp extension
 * ��Ϣ����:
 * ��Ϣ˵��:
 */
typedef struct _SVS_RTP_HEADER
{
    //�����ǰ�λ�����ǰ�����ͳһ����
    //uint16_t BitSeq;
    std::bitset<2> V;                     // V��ʶλ
    std::bitset<1> P;                     // P��ʶλ
    std::bitset<1> X;                     // X��ʶλ
    std::bitset<4> CC;                    // CC��ʶλ
    std::bitset<1> M;                     // M��ʶλ
    std::bitset<7> PT;                    // PT��ʶλ
    uint16_t  SequenceNumber;     // sequence number
    uint32_t   TimesTamp;          // timestamp
}SVS_RTP_HEADER;

/*
 * ��Ϣ����: header extension
 * ��Ϣ����:
 * ��Ϣ˵��:
 */
typedef struct _SVS_RTP_EXTEN_HEADER
{
    char          Profile[PROFILE_LEN]; // defined by peofile
    uint16_t  Length;               // length
    uint16_t  ExVersion;      // ��չ�汾����ǰֵΪ1
    uint16_t  ExType;         // ��չ����,�μ�SVS_RTP_EXTEN_DATA_TYPE
    uint32_t   StreamID;       // ��ID
}SVS_RTP_EXTEN_HEADER;


/*
 * ��Ϣ����: 0x0000  ����ý����
 * ��Ϣ����:
 * ��Ϣ˵��:
 */
typedef struct _SVS_RTP_EXTEN_STREAM_NORMAL
{
    SVS_RTP_HEADER   RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
    uint32_t    ulFrameID;          // ֡ID
    uint8_t    ucPackCount;        // ��֡�а��ĸ���
    uint8_t    ucPackID;           // ��֡�а���ID����0��ʼ
    uint8_t    ucIsKeyFrane;       // �Ƿ�Ϊ�ؼ�֡
    uint8_t    ucReserved;         // �����ֶΡ����ڱ�֤32λ���롣
}SVS_RTP_EXTEN_STREAM_NORMAL;


/*
 * ��Ϣ����: 0x0001  EOS
 * ��Ϣ����:
 * ��Ϣ˵��: ��Ϣ��Ϊ��
 */
typedef struct _SVS_RTP_EXTEN_STREAM_EOS
{
    SVS_RTP_HEADER RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
}SVS_RTP_EXTEN_STREAM_EOS;

/*
 * ��Ϣ����: 0x0101  NAT��Խ������Ϣ
 * ��Ϣ����:
 * ��Ϣ˵��:
 */
typedef struct _SVS_RTP_EXTEN_NAT_REQ
{
    SVS_RTP_HEADER  RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
    uint32_t   ulMsgReqSeq;         // ������Ϣ���к�
    uint8_t   Authorization[AUTHORIZATION_LEN];   // ��֤��Ϣ,16�ֽ�
    uint32_t   ulLocalIP;           // ý������NAT��Խǰ�ı���IP��ַ
    uint16_t  usLocalPort;         // ý������NAT��Խǰ�ı��ض˿�
    uint16_t  Resvered;            // �����ֶΡ����ڱ�֤32λ����

}SVS_RTP_EXTEN_NAT_REQ;

/*
 * ��Ϣ����: 0x0102  NAT��Խ��Ӧ��Ϣ
 * ��Ϣ����:
 * ��Ϣ˵��:
 */
typedef struct _SVS_RTP_EXTEN_NAT_RESP
{
    SVS_RTP_HEADER  RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
    uint32_t   ulMsgReqSeq;    // ������Ϣ���к�
    uint32_t   ulLocalIP;      // ý������NAT��Խǰ�ı���IP��ַ
    uint16_t  usLocalPort;    // ý������NAT��Խǰ�ı��ض˿�
    uint32_t   ulSrcIP;        // ý������NAT��Խ���IP��ַ
    uint16_t  usSrcPort;      // ý������NAT��Խ��Ķ˿�
}SVS_RTP_EXTEN_NAT_RESP;

#pragma pack(pop)

#endif /* __SVS_RTP_H__ */

