/*
 * CMduCommonResp.cpp
 *
 *  Created on: 2010-12-31
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_vms_common_resp.h"

CMduCommonResp::CMduCommonResp()
{
    m_pResp = NULL;
}

CMduCommonResp::~CMduCommonResp()
{
    m_pResp = NULL;
}

// ��ݴ������Ϣ���ʼ����Ϣ�� ֻ��ʼ����Ϣͷ��������Ҫ����
int32_t CMduCommonResp::create(char* pMsgData, uint32_t unLength)
{
    // �ȵ��ø���ĳ�ʼ���ӿ�
    int32_t nRet = CMduSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pResp = (SVS_MSG_COMMON_RESP*)(void*)getBinaryData();

    return checkMessage();
}

int32_t CMduCommonResp::checkMessage()
{
    // ���ø����create����ʱ�Ѿ��������Ϣͷ�Ƿ���ȷ

    // ����ֻ���ַ���Ϣ����Ч��
    if (sizeof(SVS_MSG_COMMON_RESP) != getLength())
    {
        SVS_LOG((SVS_LM_WARNING,
            "CMduCommonResp::checkMessage fail, msg len [%d] is incorrect, expect[%d].",
            getLength(),
            sizeof(SVS_MSG_COMMON_RESP)));
        return RET_FAIL;
    }

    return RET_OK;
}

uint32_t CMduCommonResp::getMsgType()
{
    if (NULL == m_pResp)
    {
        return SVS_MSG_TYPE_COMMON_RESP;
    }

    return m_pResp->MsgHeader.MsgType;
}

int32_t CMduCommonResp::handleMessage()
{
    // ��ӡ�¼���
    SVS_LOG((SVS_LM_INFO,
        "CMduCommonResp::handleMessage success, msg type[%#x].",
        getMsgType()));
    return RET_OK;

}

void CMduCommonResp::dump() const
{
    if (NULL == m_pResp)
    {
        return;
    }

    // �ȴ�ӡ��Ϣͷ
    CMduSvsMessage::dump();

    // ����Ϣ�ṹ��ӡ��Ϣ��
    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));

    SVS_LOG((SVS_LM_DEBUG,"\tRequestType: %#x", m_pResp->RequestType));
    SVS_LOG((SVS_LM_DEBUG,"\tRespCode: %#x", m_pResp->RespCode));

    return;
}
