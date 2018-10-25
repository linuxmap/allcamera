#ifndef _SVS_Recv_Stream_h
#define _SVS_Recv_Stream_h

#include "svs_private_msg.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_lens.h"
#include "svs_recv_rt_media_buffer.h"

// �����֡ģʽ��Ĭ��ÿ10��֡��һ���ؼ�֡
#define DEFAULT_FRAME_LOST_NUM      5

const uint32_t Frame_Lost_Num[DEFAULT_FRAME_LOST_NUM] = {10,    // 10%��֡��
                                                               5,      // 20%��֡��
                                                               4,      // 25%��֡��
                                                               3,      // 33%��֡��
                                                               2};     // 50%��֡��
/**
 * class ��ý����ջ���
 */
class CAC_Recv_Stream : public ACE_Event_Handler
{
public:
    CAC_Recv_Stream(ACE_Reactor *pRecvReactor,
                                CSVS_Recv_RT_Media_Buffer *pMediaBuffer,
                                uint32_t recvThreadNo );
    virtual ~CAC_Recv_Stream();

    // ��ʼ��
    int32_t init(const char *szDevID);

    // ��ʼ���գ���Ҫָ��·��ID��Զ�˵�ַ
    int32_t start_recv(uint32_t ulLocalIndex,
                        int32_t nType,
                        uint32_t ulRouteID,
                        const ACE_INET_Addr &local_addr,
                        const ACE_INET_Addr &remote_addr);

    // ֹͣ���գ�����ǰ���ݷ���ý�崦����
    int32_t stop_recv();

    // ���ó�֡¼��ģʽ
    void record_key_frame(bool bSwitch);

    // ���ûط�ʱ��ƫ��
    void set_time_remove(time_t remove_time)
    {
        recv_time_remove_ = remove_time;
    }

    // �˺�������ʵ�� -- ���򲻿��á�
    virtual ACE_HANDLE get_handle(void) const
    {
        return endpoint_.get_handle();
    }

    // ���ؽ��յ�ַ
    void get_local_addr( ACE_INET_Addr &addr ) const
    {
        (void)endpoint_.get_local_addr(addr);
    }

    // ��ȡVTDU��ַ
    void get_remote_addr( ACE_INET_Addr &addr ) const
    {
        (void)endpoint_.get_remote_addr(addr);
    }

    virtual int32_t handle_input(ACE_HANDLE handle);

    virtual int32_t handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask mask);

    bool check_pack_head(const AVS_PACK_HEAD * pHeader)const;

    int32_t  check_status();

    //1 TODO ��ʵ��telentͳ����Ϣ
    uint32_t recv_error_packet_num()const{return 0;}
    uint32_t recv_packet_num()const{return 0;}
    uint32_t lost_frame_num()const{return 0;}
    void recv_rate_info(time_t & statStartTime, time_t & statEndTime,
                           uint32_t & recvByteNum, uint32_t & lostFrameNum,
                           uint32_t & totalFrameNum);
    time_t last_recv_full_frame_time()const
    {
        time_t lastRecvFullFrameTime
                = m_recv_first_fram_time + (time_t)(m_last_frame_timetick - m_first_frame_timetick) / 1000;
        return lastRecvFullFrameTime;
    }
    uint8_t status()const{return (uint8_t)rt_stream_state_;}
    uint32_t GetLastError()const{return m_ulLastError;}
protected:
    CAC_Recv_Stream();
    int32_t  recv_media_response();

    // ����:��ʼ��ý����ջ�����
    int32_t init_media_buffer();

    // ��������ȡ��һ����ͷ�������֡���ܳ��ȣ�������ͷ��
    int32_t get_cur_frame_size();

    void enqueue_dio( bool bLastBufferFlag = false );

    int32_t combin_packet();

    void check_record_time_slice();

    int32_t check_timetick();


    // ���´���Ϣ
    int32_t updateCluster(time_t nowTime);

    // ����¼����ļ���Ϣ
    int32_t updateRecordFile(time_t nowTime);

    void count_frame_info(bool addIndexFlag);

    void reset();

    int32_t recv_media_data();
    void print_recv_info();
    int32_t enqueue_tmpfs();
private:
    enum RT_STREAM_STATE
    {
        RT_STREAM_STATE_CLOSE                = 0x01,     // �ر�״̬
        RT_STREAM_STATE_INIT                 = 0x02,     // ��ʼ��״̬
        RT_STREAM_STATE_RECV_VTDU_RESP       = 0x03,     // ����VTDU��Ӧ״̬
        RT_STREAM_STATE_RECV_STREAM          = 0x04,     // ��������ý����״̬
    };

private:      // ������Ϣ

    volatile uint32_t    rt_stream_state_;     // ʵʱ��״̬
    uint32_t             local_index_;         // ���������������
    int32_t                       recv_type_;           // ��������
    char                      device_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];      // �豸ID
    ACE_SOCK_Stream           endpoint_;           // �ɴ����̸߳���
    uint32_t             router_id_;          // ·��ID
    ACE_INET_Addr             local_addr_;         // ���ص�ַ
    ACE_INET_Addr             remote_addr_;        // Զ�˵�ַ����ʼ����ʱ����㴫��
    CSVS_Recv_RT_Media_Buffer *p_media_buffer_;   // ý����������

    // �����˳���־
    volatile bool             m_StopRecvFlag;
private:    // ͳ����Ϣ
    ACE_Message_Block *recv_rt_media_buff_;          // ��ǰ���ջ�����
    uint32_t     remain_recv_buff_size_;        // ��ǰʣ�໺������С�������õ�������СΪ��
    uint32_t     cur_recv_frame_id_;            // ��ǰ�����յ�֡ID
    uint32_t     last_recv_frame_id_;
    uint32_t     cur_frame_remain_size_;       // ��ǰҪ���յ������������Ϊ0��Ҫ�Ӱ�ͷ�������֡��
    time_t            cur_frame_start_recv_time_;  // ��ǰ֡��ʼ���յ�ʱ��
    time_t            recv_time_remove_;             // ¼����ƽ̨ʱ���ƫ����
    time_t            start_recv_time_;              // ��ʼʱ��
    time_t            last_recv_packet_time_;       // ���һ�ν���ʱ��
    uint32_t     alloc_buffer_failure_times_; // ���仺����ʧ�ܴ���

    time_t            last_check_index_time_;       // ���һ�θ���������ʱ��
    time_t            last_check_record_time_;     // ���һ���л�¼���ļ���ʱ��

    bool              record_key_frame_flag_;      // �Ƿ�������֡¼���־
    uint32_t     record_key_frame_rate_;      // ��֡��
    uint32_t     recv_frame_num_then_lost_;   // ��֡ģʽ�µ�ǰ���յ��İ���(����Ƿ���Ҫ������)

    SVS_ERROR_MSG     m_MediaResp;                 // VTDU����Ӧ��ֻ��һ��
    size_t            m_MediaRespLen;              // ���յ�����Ӧ����

    SVS_Ingress_Stat  ingress_stat_;               // ����ͳ�ƶ���

   AVS_PACK_HEAD     m_packHeader;                // ���ݰ�ͷ�����ڻ�ȡһ��֡��Сʱ

   time_t m_recv_first_fram_time;
   uint32_t m_first_frame_timetick;
   uint32_t m_last_frame_timetick;
   uint32_t m_last_Adjust_timetick;
   uint32_t m_break_frame_num;

    ACE_Time_Value last_print_time_;
    volatile uint32_t recv_byte_num_;
    volatile uint32_t lost_frame_num_;
    volatile uint32_t total_frame_num_;
    uint32_t m_ulRecvThreadNo; // �����̺߳�

    uint32_t m_ulLastError;


};
#endif //_SVS_Recv_Stream_h


