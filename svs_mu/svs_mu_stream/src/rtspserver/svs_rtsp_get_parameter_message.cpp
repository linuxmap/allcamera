#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_rtsp_defs.h"
#include "svs_rtsp_get_parameter_message.h"
#include "svs_rtsp_protocol.h"

CRtspGerParamMessage::CRtspGerParamMessage()
{
    m_unMethodType     = RTSP_METHOD_GETPARAMETER;
}
CRtspGerParamMessage::~CRtspGerParamMessage()
{

}

