#ifndef _SVS_Recv_Stream_h
#define _SVS_Recv_Stream_h

#include "svs_private_msg.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_lens.h"
#include "svs_recv_rt_media_buffer.h"

// 定义抽帧模式下默认每10个帧丢一个关键帧
#define DEFAULT_FRAME_LOST_NUM      5

const uint32_t Frame_Lost_Num[DEFAULT_FRAME_LOST_NUM] = {10,    // 10%丢帧率
                                                               5,      // 20%丢帧率
                                                               4,      // 25%丢帧率
                                                               3,      // 33%丢帧率
                                                               2};     // 50%丢帧率
/**
 * class 流媒体接收基类
 */
class CAC_Recv_Stream : public ACE_Event_Handler
{
public:
    CAC_Recv_Stream(ACE_Reactor *pRecvReactor,
                                CSVS_Recv_RT_Media_Buffer *pMediaBuffer,
                                uint32_t recvThreadNo );
    virtual ~CAC_Recv_Stream();

    // 初始化
    int32_t init(const char *szDevID);

    // 开始接收，需要指定路由ID和远端地址
    int32_t start_recv(uint32_t ulLocalIndex,
                        int32_t nType,
                        uint32_t ulRouteID,
                        const ACE_INET_Addr &local_addr,
                        const ACE_INET_Addr &remote_addr);

    // 停止接收，将当前数据放入媒体处理器
    int32_t stop_recv();

    // 设置抽帧录像模式
    void record_key_frame(bool bSwitch);

    // 设置回放时间偏移
    void set_time_remove(time_t remove_time)
    {
        recv_time_remove_ = remove_time;
    }

    // 此函数必须实现 -- 否则不可用。
    virtual ACE_HANDLE get_handle(void) const
    {
        return endpoint_.get_handle();
    }

    // 本地接收地址
    void get_local_addr( ACE_INET_Addr &addr ) const
    {
        (void)endpoint_.get_local_addr(addr);
    }

    // 获取VTDU地址
    void get_remote_addr( ACE_INET_Addr &addr ) const
    {
        (void)endpoint_.get_remote_addr(addr);
    }

    virtual int32_t handle_input(ACE_HANDLE handle);

    virtual int32_t handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask mask);

    bool check_pack_head(const AVS_PACK_HEAD * pHeader)const;

    int32_t  check_status();

    //1 TODO 请实现telent统计信息
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

    // 功能:初始化媒体接收缓冲区
    int32_t init_media_buffer();

    // 从连接上取到一个包头，计算出帧的总长度（包含包头）
    int32_t get_cur_frame_size();

    void enqueue_dio( bool bLastBufferFlag = false );

    int32_t combin_packet();

    void check_record_time_slice();

    int32_t check_timetick();


    // 更新簇信息
    int32_t updateCluster(time_t nowTime);

    // 更新录像大文件信息
    int32_t updateRecordFile(time_t nowTime);

    void count_frame_info(bool addIndexFlag);

    void reset();

    int32_t recv_media_data();
    void print_recv_info();
    int32_t enqueue_tmpfs();
private:
    enum RT_STREAM_STATE
    {
        RT_STREAM_STATE_CLOSE                = 0x01,     // 关闭状态
        RT_STREAM_STATE_INIT                 = 0x02,     // 初始化状态
        RT_STREAM_STATE_RECV_VTDU_RESP       = 0x03,     // 接收VTDU响应状态
        RT_STREAM_STATE_RECV_STREAM          = 0x04,     // 正常接收媒体流状态
    };

private:      // 基本信息

    volatile uint32_t    rt_stream_state_;     // 实时流状态
    uint32_t             local_index_;         // 处理器分配的索引
    int32_t                       recv_type_;           // 接收类型
    char                      device_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];      // 设备ID
    ACE_SOCK_Stream           endpoint_;           // 由创建线程更新
    uint32_t             router_id_;          // 路由ID
    ACE_INET_Addr             local_addr_;         // 本地地址
    ACE_INET_Addr             remote_addr_;        // 远端地址，开始接收时由外层传入
    CSVS_Recv_RT_Media_Buffer *p_media_buffer_;   // 媒体流缓存区

    // 接收退出标志
    volatile bool             m_StopRecvFlag;
private:    // 统计信息
    ACE_Message_Block *recv_rt_media_buff_;          // 当前接收缓冲区
    uint32_t     remain_recv_buff_size_;        // 当前剩余缓冲区大小，以配置的条带大小为限
    uint32_t     cur_recv_frame_id_;            // 当前正接收的帧ID
    uint32_t     last_recv_frame_id_;
    uint32_t     cur_frame_remain_size_;       // 当前要接收的数据量，如果为0需要从包头里解析出帧长
    time_t            cur_frame_start_recv_time_;  // 当前帧开始接收的时间
    time_t            recv_time_remove_;             // 录像与平台时间的偏移量
    time_t            start_recv_time_;              // 开始时间
    time_t            last_recv_packet_time_;       // 最后一次接收时间
    uint32_t     alloc_buffer_failure_times_; // 分配缓冲区失败次数

    time_t            last_check_index_time_;       // 最后一次更新索引的时间
    time_t            last_check_record_time_;     // 最后一次切换录像文件的时间

    bool              record_key_frame_flag_;      // 是否启动抽帧录像标志
    uint32_t     record_key_frame_rate_;      // 抽帧率
    uint32_t     recv_frame_num_then_lost_;   // 抽帧模式下当前已收到的包数(检查是否需要丢包了)

    SVS_ERROR_MSG     m_MediaResp;                 // VTDU的响应，只用一次
    size_t            m_MediaRespLen;              // 接收到的响应长度

    SVS_Ingress_Stat  ingress_stat_;               // 接收统计对象

   AVS_PACK_HEAD     m_packHeader;                // 数据包头，用在获取一下帧大小时

   time_t m_recv_first_fram_time;
   uint32_t m_first_frame_timetick;
   uint32_t m_last_frame_timetick;
   uint32_t m_last_Adjust_timetick;
   uint32_t m_break_frame_num;

    ACE_Time_Value last_print_time_;
    volatile uint32_t recv_byte_num_;
    volatile uint32_t lost_frame_num_;
    volatile uint32_t total_frame_num_;
    uint32_t m_ulRecvThreadNo; // 接收线程号

    uint32_t m_ulLastError;


};
#endif //_SVS_Recv_Stream_h


