#ifndef NETWORKSERVER_H
#define NETWORKSERVER_H
#include <sys/socket.h>
#include <unistd.h>
#include "errno.h"
#include "posix/intercept.h"
#include "messaging/IMessageManager.h"
#include "messaging/Pack.h"
#include "Logging.h"
#include "runtime/DiggiAPI.h"



class NetworkServer {
private:
    IDiggiAPI *diggiapi;

public:
    //NetworkServer(IMessageManager *pMsgManager, ILog *pLogObj);
    NetworkServer(IDiggiAPI* mman);
    ~NetworkServer();

    void InitializeServer();

    static void ServerSocket(void *message, int status);
    static void ServerBind(void *message, int status);
    static void ServerGetsockname(void *msg, int status);
    static void ServerConnect(void *msg, int status);
    static void ServerFcntl(void *msg, int status);
    static void ServerSetsockopt(void *msg, int status);
    static void ServerGetsockopt(void *msg, int status);
    static void ServerListen(void *msg, int status);
    static void ServerSelect(void *msg, int status);
    static void ServerRecv(void *msg, int status);
    static void ServerSend(void *msg, int status);
    static void ServerAccept(void *msg, int status);
    static void ServerClose(void *msg, int status);
    static void ServerRand(void *msg, int status);
    

/*protected:
    IMessageManager *apMessageManager;
	ILog *apLog;
*/
};

#endif