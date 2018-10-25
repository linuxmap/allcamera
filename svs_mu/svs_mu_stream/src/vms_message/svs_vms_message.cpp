/*
 * CMduSvsMessage.cpp
 *
 *  Created on: 2010-12-30
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_vms_message.h"
#include "svs_adapter_config.h"
#include "svs_vms_msg_factory.h"

CMduSvsMessage::CMduSvsMessage()
{
    m_unMsgLength   = 0;
    m_pMsgHeader    = NULL;
    m_pBinaryData   = NULL;
    m_bDeleteBuffer = false;
}

CMduSvsMessage::~CMduSvsMessage()
{
    if (m_bDeleteBuffer)
    {
        delete []m_pBinaryData;
    }

    m_unMsgLength = 0;
    m_pMsgHeader = NULL;
    m_pBinaryData = NULL;
    m_bDeleteBuffer = false;
}

int32_t CMduSvsMessage::create(char* pMsgData, uint32_t unLength)
{
    if (NULL == pMsgData)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSvsMessage::create fail, msg is null."));
        return RET_ERR_PARAM;
    }

    if (sizeof(SVS_MSG_HEADER) > unLength)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSvsMessage::create fail, len [%u] is too short.",
            unLength));
        return RET_ERR_PARAM;
    }

    m_pBinaryData   = pMsgData;
    m_pMsgHeader    = (SVS_MSG_HEADER*)(void*)pMsgData;
    m_unMsgLength   = unLength;
    m_bDeleteBuffer = false;

    return RET_OK;
}

//lint -e429
int32_t CMduSvsMessage::create(uint32_t unLength,
                        uint32_t unTransNo)
{
    if ((0 == unLength) || (sizeof(SVS_MSG_HEADER) >= unLength))
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSvsMessage::create fail, len [%d] is invalid.",
            unLength));
        return RET_ERR_PARAM;
    }

    char *pBuffer = NULL;
    try
    {
        pBuffer = new char[unLength];
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSvsMessage::create fail, alloc msg buffer[%u] fail.",
            unLength));
        return RET_ERR_SYS_NEW;
    }

    memset(pBuffer, 0x0, unLength);

    m_pBinaryData   = pBuffer;
    m_pMsgHeader    = (SVS_MSG_HEADER*)(void*)pBuffer;
    m_unMsgLength   = unLength;
    m_bDeleteBuffer = true;

    return initHeader(unTransNo);
}
//lint +e429

int32_t CMduSvsMessage::checkMessage()
{
    // ��鹫���ֶ�
    if (NULL == m_pMsgHeader)
    {
        return SVS_MSG_MU_INTERNAL_ERR;
    }

    if (SVS_MSG_PROTOCOL_VERSION != m_pMsgHeader->ProtocolVersion)
    {
        return SVS_MSG_VERSION_ERR;
    }

    if (m_pMsgHeader->PacketLength != m_unMsgLength)
    {
        return SVS_MSG_LENGTH_ERR;
    }

    return RET_OK;
}


/*lint -e1763*/
char* CMduSvsMessage::getBinaryData() const
{
    return m_pBinaryData;
}
/*lint +e1763*/

uint32_t CMduSvsMessage::getLength()const
{
    return m_unMsgLength;
}

// ���ֵ�ķ�Χ
// �������ֵ����Сֵ
int32_t CMduSvsMessage::checkValueRange
(
    uint32_t ulValue,
    uint32_t ulMinValue,
    uint32_t ulMaxValue
)const
{
    // С�ڵ�����Сֵ
    if (ulValue <= ulMinValue)
    {
        return RET_FAIL;
    }

    // ���ڵ������ֵ
    if (ulValue >= ulMaxValue)
    {
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CMduSvsMessage::checkTransDirection(uint32_t unPeerType, uint32_t unTransDirection) const
{
    if (PEER_TYPE_PU == unPeerType)
    {
        if ((TRANS_DIRECTION_RECVONLY == unTransDirection)
          ||(TRANS_DIRECTION_SENDRECV == unTransDirection))
        {
            return RET_OK;
        }
    }
    else if (PEER_TYPE_CU == unPeerType)
    {
        if (TRANS_DIRECTION_RECVONLY == unTransDirection)
        {
            return RET_OK;
        }
    }
    else if (PEER_TYPE_RECORD == unPeerType)
    {
        if (TRANS_DIRECTION_RECVONLY == unTransDirection)
        {
            return RET_OK;
        }
    }
    else if (PEER_TYPE_STREAM == unPeerType)
    {
        if (TRANS_DIRECTION_RECVONLY == unTransDirection)
        {
            return RET_OK;
        }
    }

    return RET_FAIL;
}

void CMduSvsMessage::dump() const
{
    // ������Ϣֻ��ӡSVS_MSG_HEADER
    if (NULL == m_pMsgHeader)
    {
        return;
    }

    SVS_LOG((SVS_LM_DEBUG,"MessageHeader:"));
    SVS_LOG((SVS_LM_DEBUG,"\tProtocolVersion: %#x", m_pMsgHeader->ProtocolVersion));
    SVS_LOG((SVS_LM_DEBUG,"\tMsgType: %#x", m_pMsgHeader->MsgType));


    SVS_LOG((SVS_LM_DEBUG,"\tTransactionNo: %#x", m_pMsgHeader->TransactionNo));
    SVS_LOG((SVS_LM_DEBUG,"\tPacketLength: %d", m_pMsgHeader->PacketLength));

    return;
}

/****************����Ϊ�ڲ�����ʵ��****************/
int32_t CMduSvsMessage::initHeader(uint32_t unTransNo)
{
    if (NULL == m_pMsgHeader)
    {
        return RET_FAIL;
    }
    m_pMsgHeader->MsgType         = (uint32_t)getMsgType();
    m_pMsgHeader->ProtocolVersion = SVS_MSG_PROTOCOL_VERSION;
    m_pMsgHeader->PacketLength    = m_unMsgLength;
    if (0 != unTransNo)
    {
        m_pMsgHeader->TransactionNo = CMduMsgFactory::instance()->getRespTransactionNo(unTransNo);
    }
    else
    {
        m_pMsgHeader->TransactionNo = CMduMsgFactory::instance()->getReqTransactionNo();
    }


    return RET_OK;
}
