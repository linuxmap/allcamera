#include "svs_log_msg.h"
#include "svs_adapter_mpeg_ts_enc.h"
#include "svs_adapter_rtp_packet.h"

//#define __DEBUG_WRITE_TS__

CMpegTsEnc::CMpegTsEnc()
{
    m_pPsStreamHandle           = NULL;
    m_pUserData                 = NULL;

    m_pPsBuffer                 = NULL;            // TS数据缓冲区S数据缓冲区
    m_unBufferSize              = MPEG_BUFFER_SIZE;// TS数据缓冲区大小

    Reset();
}

CMpegTsEnc::~CMpegTsEnc()
{

}

// 封装器初始化
int32_t CMpegTsEnc::init(CMpegTsEncHandle* pHandle, void* pUserData)
{

    return 0;
}



void CMpegTsEnc::release()
{

}

//打开
int32_t CMpegTsEnc::open()
{


    return 0;
}

//关闭
void CMpegTsEnc::close()
{

    Reset();
}


// 写入一个视频基本流
int32_t CMpegTsEnc::writeVideoEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp,bool bKeyFrame)
{

    return 0;
}

// 写入一个音频基本流
int32_t CMpegTsEnc::writeAudioEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp)
{

    return 0;
}

// 回到写入一个ES数据
int CMpegTsEnc::write_buffer(void *opaque, uint8_t *buf, int buf_size)
{
    CMpegTsEnc* pTsEnc = (CMpegTsEnc*)opaque;

    return pTsEnc->write_ts_data(buf,buf_size);
}

//写入TS流数据

int CMpegTsEnc::write_ts_data(uint8_t *buf, int buf_size)
{

    return 0;
}
void CMpegTsEnc::Reset()
{
    m_ulTimeTick                = 0;
    m_ulLastTimeTick           = 0;
}



