/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_QUEUEHANDLER
#define VC4CL_QUEUEHANDLER

#include "Event.h"

namespace vc4cl
{
    class CommandQueue;

    void pushEventToQueue(Event* event);
    object_wrapper<Event> peekQueue(CommandQueue* queue);
    void waitForEvent(const Event* event);
    void initEventQueue();
    void deinitEventQueue();

} /* namespace vc4cl */

#endif /* VC4CL_QUEUEHANDLER */
