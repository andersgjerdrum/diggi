/**
 * @file signalhandler.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Signal handler implementation
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "runtime/SignalHandler.h"

/**
 * @brief Construct a new Diggi Signal Handler:: Diggi Signal Handler object
 * 
 * @param mm thread safe message manger for issuing signals to host untrustsed runtime.
 */
DiggiSignalHandler::DiggiSignalHandler(IThreadSafeMM *mm):messagingService(mm)
{ }
    
/**
 * @brief invoked by instance to shut down.
 * Causes trusted runtime to send message with signal_type to the untrusted runtime, 
 * causes a voulentary shutdown of instance. 
 * Results in func_stop() callback for signaling instance, invoked out-of-band by untrusted runtime thread.
 * @see func_stop
 * 
 */
void DiggiSignalHandler::Stopfunc()
{
    /*
        No need for addressing or encryption, 
        only signal runtime to exit func.
    */
    aid_t dummy;
    /*
        causes it to be handled by the runtime queue, instead of a target func queue
    */
    dummy.raw = 123456789;
    auto msg = messagingService->getIAsyncMessageManager()->allocateMessage(dummy, dummy, 8, REGULAR);
    msg->type = DIGGI_SIGNAL_TYPE_EXIT;
    messagingService->getIAsyncMessageManager()->sendMessage(msg);
}
