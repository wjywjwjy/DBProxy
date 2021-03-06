#include <assert.h>
#include "Client.h"

#include "protocol/SSDBProtocol.h"
#include "SSDBWaitReply.h"

static const std::string SSDB_OK = "ok";
static const std::string SSDB_ERROR = "error";

static void syncSSDBStrList(std::shared_ptr<ClientSession>& client, const std::vector<std::string>& strList)
{
    SSDBProtocolRequest& strsResponse = client->getCacheSSDBProtocol();
    strsResponse.init();

    strsResponse.writev(strList);
    strsResponse.endl();

    client->sendPacket(strsResponse.getResult(), strsResponse.getResultLen());
}

StrListSSDBReply::StrListSSDBReply(std::shared_ptr<ClientSession> client) : BaseWaitReply(client)
{
}

void StrListSSDBReply::onBackendReply(int64_t dbServerSocketID, BackendParseMsg&)
{

}

void StrListSSDBReply::mergeAndSend(std::shared_ptr<ClientSession>& client)
{
    mStrListResponse.endl();
    client->sendPacket(mStrListResponse.getResult(), mStrListResponse.getResultLen());
}

void StrListSSDBReply::pushStr(std::string&& str)
{
    mStrListResponse.writev(str);
}

void StrListSSDBReply::pushStr(const std::string& str)
{
    mStrListResponse.writev(str);
}

void StrListSSDBReply::pushStr(const char* str)
{
    mStrListResponse.appendStr(str);
}

SSDBSingleWaitReply::SSDBSingleWaitReply(std::shared_ptr<ClientSession> client) : BaseWaitReply(client)
{
}

void SSDBSingleWaitReply::onBackendReply(int64_t dbServerSocketID, BackendParseMsg& msg)
{
    for (auto& v : mWaitResponses)
    {
        if (v.dbServerSocketID == dbServerSocketID)
        {
            v.responseBinary = msg.transfer();
            break;
        }
    }
}

void SSDBSingleWaitReply::mergeAndSend(std::shared_ptr<ClientSession>& client)
{
    if (mErrorCode != nullptr)
    {
        syncSSDBStrList(client, { SSDB_ERROR, *mErrorCode });
    }
    else
    {
        client->sendPacket(mWaitResponses.front().responseBinary);
    }
}

SSDBMultiSetWaitReply::SSDBMultiSetWaitReply(std::shared_ptr<ClientSession> client) : BaseWaitReply(client)
{
}

void SSDBMultiSetWaitReply::onBackendReply(int64_t dbServerSocketID, BackendParseMsg& msg)
{
    for (auto& v : mWaitResponses)
    {
        if (v.dbServerSocketID == dbServerSocketID)
        {
            v.responseBinary = msg.transfer();

            if (mWaitResponses.size() != 1)
            {
                v.ssdbReply = new SSDBProtocolResponse;
                v.ssdbReply->parse(v.responseBinary->c_str());
            }

            break;
        }
    }
}

void SSDBMultiSetWaitReply::mergeAndSend(std::shared_ptr<ClientSession>& client)
{
    if (mErrorCode != nullptr)
    {
        syncSSDBStrList(client, { SSDB_ERROR, *mErrorCode });
    }
    else
    {
        if (mWaitResponses.size() == 1)
        {
            client->sendPacket(mWaitResponses.front().responseBinary);
        }
        else
        {
            std::shared_ptr<std::string> errorReply = nullptr;
            int64_t num = 0;

            for (auto& v : mWaitResponses)
            {
                int64_t tmp;
                if (read_int64(v.ssdbReply, &tmp).ok())
                {
                    num += tmp;
                }
                else
                {
                    errorReply = v.responseBinary;
                    break;
                }
            }

            if (errorReply != nullptr)
            {
                client->sendPacket(errorReply);
            }
            else
            {
                SSDBProtocolRequest& response = client->getCacheSSDBProtocol();
                response.init();

                response.writev(SSDB_OK, num);
                response.endl();
                client->sendPacket(response.getResult(), response.getResultLen());
            }
        }
    }
}

SSDBMultiGetWaitReply::SSDBMultiGetWaitReply(std::shared_ptr<ClientSession> client) : BaseWaitReply(client)
{
}

void SSDBMultiGetWaitReply::onBackendReply(int64_t dbServerSocketID, BackendParseMsg& msg)
{
    for (auto& v : mWaitResponses)
    {
        if (v.dbServerSocketID == dbServerSocketID)
        {
            v.responseBinary = msg.transfer();

            if (mWaitResponses.size() != 1)
            {
                v.ssdbReply = new SSDBProtocolResponse;
                v.ssdbReply->parse(v.responseBinary->c_str());
            }

            break;
        }
    }
}

void SSDBMultiGetWaitReply::mergeAndSend(std::shared_ptr<ClientSession>& client)
{
    if (mErrorCode != nullptr)
    {
        syncSSDBStrList(client, { SSDB_ERROR, *mErrorCode });
    }
    else
    {
        if (mWaitResponses.size() == 1)
        {
            client->sendPacket(mWaitResponses.front().responseBinary);
        }
        else
        {
            std::shared_ptr<std::string> errorReply = nullptr;

            std::vector<Bytes> kvs;

            for (auto& v : mWaitResponses)
            {
                if (!read_bytes(v.ssdbReply, &kvs).ok())
                {
                    errorReply = v.responseBinary;
                    break;
                }
            }

            if (errorReply != nullptr)
            {
                client->sendPacket(errorReply);
            }
            else
            {
                SSDBProtocolRequest& strsResponse = client->getCacheSSDBProtocol();
                strsResponse.init();

                strsResponse.writev(SSDB_OK);
                for (auto& v : kvs)
                {
                    strsResponse.appendStr(v.buffer, v.len);
                }
                
                strsResponse.endl();
                client->sendPacket(strsResponse.getResult(), strsResponse.getResultLen());
            }
        }
    }
}