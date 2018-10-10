#include "async_state_machine.h"

namespace cblp {



///*virtual*/ async_callback_state_machine::~async_callback_state_machine()
//{
//#if 0
//    if (deadline_timer ) {
//        deadline_timer->cancel();
//        deadline_timer.reset();
//    }
//#endif
//}


//async_callback_state_machine::async_callback_state_machine(boost::asio::io_service& io_service
//                                                           , int64_t timeout
//                                                           , async_callback_state_machine::callback_type finalizer)
//    : io_service(io_service)
//    , timeout_msec(timeout)
//    , deadline_timer( timeout_msec > 0
//                      ? new boost::asio::deadline_timer(io_service,
//                                                        boost::posix_time::milliseconds(timeout))
//                      : nullptr)
//    , final_callback(finalizer)
//    , strand(io_service)
//    , timestamp(std::chrono::high_resolution_clock::now())
//{}


//void async_callback_state_machine::deadline_handler(const boost::system::error_code& ec,
//                                                           boost::shared_ptr<async_callback_state_machine>& machine)
//{
//    if ( ec == boost::asio::error::operation_aborted )
//        return;
//    machine->final_callback(call_result_type::timeouted);
//}


//void async_callback_state_machine::remove_scheduled_task(boost::shared_ptr<async_callback_state_machine::i_task>& ptr)
//{
//    // TODO: add mutex
//    _mutex.lock();
//    scheduled_tasks.erase(ptr);
//    _mutex.unlock();
//}


//void async_callback_state_machine::schedule_task(boost::shared_ptr<async_callback_state_machine::i_task> task)
//{
//    boost::shared_ptr<weak_binder> wrapper(new weak_binder(task, shared_from_this()));
//    _mutex.lock();
//    scheduled_tasks.insert(task);
//    _mutex.unlock();
//    io_service.post(strand.wrap(boost::bind(&weak_binder::operator(), wrapper)));
//}

//void async_callback_state_machine::schedule_task(boost::shared_ptr<i_task> task, int timeout)
//{
//    boost::shared_ptr<timer_binder> wrapper(new timer_binder(task, shared_from_this(), timeout));
//    _mutex.lock();
//    scheduled_tasks.insert(task);
//    active_timers.insert(wrapper->timer);
//    _mutex.unlock();
//    wrapper->timer->async_wait(boost::bind(&timer_binder::timeout_handler, _1, wrapper));
//}


//void async_callback_state_machine::unschedule_task(boost::weak_ptr<i_task> task)
//{
//    boost::shared_ptr<i_task> t = task.lock();
//    if (!t)
//        return;
//    _mutex.lock();
//    scheduled_tasks.erase(t);
//    _mutex.unlock();
//    boost::shared_ptr<boost::asio::deadline_timer> timer = t->timer.lock();
//    if (timer) {
//        timer->cancel();
//        _mutex.lock();
//        active_timers.erase(timer);
//        _mutex.unlock();
//    }
//}


//void async_callback_state_machine::stop(call_result_type result /*= call_result_type::aborted*/)
//{
//    std::vector<boost::shared_ptr<boost::asio::deadline_timer>> timers;
//    deadline_timer->cancel();
//    deadline_timer.reset();

//    do {
//        boost::recursive_mutex::scoped_lock guard(_mutex);
//        if (scheduled_tasks.empty())
//            break;

//        auto task = *(scheduled_tasks.begin());
//        guard.unlock();
//        unschedule_task(task);
//        guard.lock();

//    } while(1);

//    _mutex.lock();
//    for (auto entry : active_timers)
//        timers.push_back(entry);
//    active_timers.clear();
//    _mutex.unlock();

//    for (auto timer : timers)
//        timer->cancel();

//    final_callback(result);
//}


} // namespace cblp
