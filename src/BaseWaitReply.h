#ifndef _BASE_WAIT_REPLY_H
#define _BASE_WAIT_REPLY_H

#include <string>
#include <memory>
#include <vector>
#include <mutex>

class ClientSession;
class SSDBProtocolResponse;
struct parse_tree;

struct BackendParseMsg
{
    BackendParseMsg()
    {
        redisReply = nullptr;
        responseMemory = nullptr;
    }

    std::shared_ptr<std::string>& transfer()
    {
        return responseMemory;
    }

    parse_tree* redisReply;
    std::shared_ptr<std::string>    responseMemory;
};

class BaseWaitReply
{
public:
    typedef std::shared_ptr<BaseWaitReply>  PTR;
    typedef std::weak_ptr<BaseWaitReply>    WEAK_PTR;

    BaseWaitReply(std::shared_ptr<ClientSession>& client);
    std::shared_ptr<ClientSession>&  getClient();

    virtual ~BaseWaitReply();

    void    lockReply()
    {
        mLock.lock();
    }

    void    unLockReply()
    {
        mLock.unlock();
    }
public:
    /*  收到db服务器的返回值 */
    virtual void    onBackendReply(int64_t dbServerSocketID, BackendParseMsg&) = 0;
    /*  当所有db服务器都返回数据后，调用此函数尝试合并返回值并发送给客户端  */
    virtual void    mergeAndSend(std::shared_ptr<ClientSession>&) = 0;

public:
    /*  检测是否所有等待的db服务器均已返回数据    */
    bool            isAllCompleted() const;
    /*  添加一个等待的db服务器    */
    void            addWaitServer(int64_t serverSocketID);

    bool            hasError() const;

    /*  设置出现错误    */
    void            setError(const char* errorCode);

protected:

    struct PendingResponseStatus
    {
        PendingResponseStatus()
        {
            dbServerSocketID = 0;
            ssdbReply = nullptr;
            redisReply = nullptr;
            forceOK = false;
        }

        int64_t                 dbServerSocketID;       /*  此等待的response所在的db服务器的id    */
        std::shared_ptr<std::string>    responseBinary;
        SSDBProtocolResponse*   ssdbReply;              /*  解析好的ssdb response*/
        parse_tree*             redisReply;
        bool                    forceOK;                /*  是否强制设置成功    */
    };

    std::vector<PendingResponseStatus>  mWaitResponses; /*  等待的各个服务器返回值的状态  */  /*ToOD::或许它没得线程安全问题*/

    std::shared_ptr<ClientSession>      mClient;
    std::string*                        mErrorCode;

    std::mutex                          mLock;
};

#endif