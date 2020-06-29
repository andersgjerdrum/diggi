#ifndef DIGGI_SIGNAL_HANDLER
#define DIGGI_SIGNAL_HANDLER
#include "messaging/IAsyncMessageManager.h"
#include "messaging/IThreadSafeMM.h"
#include "datatypes.h"


class ISignalHandler {
public:
	ISignalHandler() {}
	virtual ~ISignalHandler() {}
	virtual void Stopfunc() = 0;
};

#endif





class DiggiSignalHandler : public ISignalHandler{
    IThreadSafeMM *messagingService;
    public:
    DiggiSignalHandler(IThreadSafeMM *mm);
    void Stopfunc();
};