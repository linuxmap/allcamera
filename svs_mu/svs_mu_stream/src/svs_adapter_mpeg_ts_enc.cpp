#include "svs_log_msg.h"
#include "svs_adapter_mpeg_ts_enc.h"
#include "svs_adapter_rtp_packet.h"

//#define __DEBUG_WRITE_TS__

CMpegTsEnc::CMpegTsEnc()
{
    m_pPsStreamHandle           = NULL;
    m_pUserData                 = NULL;

    m_pPsBuffer                 = NULL;            // TS���ݻ�����S���ݻ�����
    m_unBufferSize              = MPEG_BUFFER_SIZE;// TS���ݻ�������С

    Reset();
}

CMpegTsEnc::~CMpegTsEnc()
{

}

// ��װ����ʼ��
int32_t CMpegTsEnc::init(CMpegTsEncHandle* pHandle, void* pUserData)
{

    return 0;
}



void CMpegTsEnc::release()
{

}

//��
int32_t CMpegTsEnc::open()
{


    return 0;
}

//�ر�
void CMpegTsEnc::close()
{

    Reset();
}


// д��һ����Ƶ������
int32_t CMpegTsEnc::writeVideoEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp,bool bKeyFrame)
{

    return 0;
}

// д��һ����Ƶ������
int32_t CMpegTsEnc::writeAudioEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp)
{

    return 0;
}

// �ص�д��һ��ES����
int CMpegTsEnc::write_buffer(void *opaque, uint8_t *buf, int buf_size)
{
    CMpegTsEnc* pTsEnc = (CMpegTsEnc*)opaque;

    return pTsEnc->write_ts_data(buf,buf_size);
}

//д��TS������

int CMpegTsEnc::write_ts_data(uint8_t *buf, int buf_size)
{

    return 0;
}
void CMpegTsEnc::Reset()
{
    m_ulTimeTick                = 0;
    m_ulLastTimeTick           = 0;
}



