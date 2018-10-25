#ifndef _SVS_GB28181_MANSCDP_H_
#define _SVS_GB28181_MANSCDP_H_

#include "tinyxml.h"
#include "svs_ace_header.h"

namespace SVS_ACM
{
    struct LENS_INFO;
    class REQUEST_DEV_CTRL;
}

class CDeviceStackGB28181;

class CManscdp
{
public:
    CManscdp()
    {
        m_pDeviceStackGB28181 = NULL;
    };
    virtual ~CManscdp(){};

    int32_t parse(const char* pszXML);
    int32_t createQueryCatalog(const char* pszDeviceID);
    int32_t createKeyFrame();
    std::string getXML();
    void setDeviceStackGB28181(CDeviceStackGB28181 &rDeviceStackGB28181);

protected:
    const char* getSN();

private:
    int32_t parseNotify(const TiXmlElement &rRoot);
    int32_t parseKeepAlive(const TiXmlElement &rRoot);
    int32_t parseAlarm(const TiXmlElement &rRoot);

    int32_t parseResponse(const TiXmlElement &rRoot);
    int32_t parseQueryCatalog(const TiXmlElement &rRoot);
    int32_t parseDeviceItem(const TiXmlElement &rItem, SVS_ACM::LENS_INFO& rLensInfo);

protected:
    CDeviceStackGB28181* m_pDeviceStackGB28181;
    TiXmlDocument m_objXmlDoc;

private:
    static ACE_Atomic_Op<ACE_Thread_Mutex, uint32_t> m_nSN;
    char m_szSN[11];    //uint32_t的最大长度
};

class CManscdpDevCtrl : public CManscdp
{
public:
    CManscdpDevCtrl(SVS_ACM::REQUEST_DEV_CTRL& rRequest) : m_rRequest(rRequest)
    {
        memset(m_szPTZCmd, 0, sizeof(m_szPTZCmd));
    }

    int32_t createDeviceControl();

private:
    int32_t setPTZCmd();
    int32_t checkPTZCmd();

private:
    enum DEV_CTRL_MAX
    {
        DEV_CTRL_MAX_ZOOM_PARAM = 0x0F
    };

    enum DEV_CTRL_POS
    {
        DEV_CTRL_POS_MAGIC          = 0,
        DEV_CTRL_POS_VERSION_CHECK  = 1,
        DEV_CTRL_POS_ADDRESS        = 2,
        DEV_CTRL_POS_CMD            = 3,
        DEV_CTRL_POS_DATA1          = 4,
        DEV_CTRL_POS_DATA2          = 5,
        DEV_CTRL_POS_DATA3_HIGH4    = 6,
        DEV_CTRL_POS_CHECK_CODE     = 7,
        DEV_CTRL_POS_MAX
    };

    enum DEV_CTRL_CMD
    {
        DEV_CTRL_CMD_STOP           = 0x00,
        DEV_CTRL_CMD_RIGHT          = 0x01,
        DEV_CTRL_CMD_LEFT           = 0x02,
        DEV_CTRL_CMD_DOWN           = 0x04,
        DEV_CTRL_CMD_UP             = 0x08,
        DEV_CTRL_CMD_ZOOM_IN        = 0x10,
        DEV_CTRL_CMD_ZOOM_OUT       = 0x20,
        DEV_CTRL_CMD_STOP_FI        = 0x40,
        DEV_CTRL_CMD_FOCAL_FAR      = 0x41,
        DEV_CTRL_CMD_FOCAL_NEAR     = 0x42,
        DEV_CTRL_CMD_APERTURE_OPEN  = 0x44,
        DEV_CTRL_CMD_APERTURE_CLOSE = 0x48,
        DEV_CTRL_CMD_PREFAB_BIT_SET = 0x81,
        DEV_CTRL_CMD_PREFAB_BIT_RUN = 0x82,
        DEV_CTRL_CMD_PREFAB_BIT_DEL = 0x83
    };

private:
    const SVS_ACM::REQUEST_DEV_CTRL& m_rRequest;
    char m_szPTZCmd[DEV_CTRL_POS_MAX * 2 + 1];
};

inline std::string CManscdp::getXML()
{
    TiXmlPrinter objPrinter;
    objPrinter.SetIndent(0);
    m_objXmlDoc.Accept(&objPrinter);  

    return objPrinter.CStr();
}

inline void CManscdp::setDeviceStackGB28181(CDeviceStackGB28181 &rDeviceStackGB28181)
{
    m_pDeviceStackGB28181 = &rDeviceStackGB28181;
}

#endif
