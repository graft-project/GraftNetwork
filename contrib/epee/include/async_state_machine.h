#ifndef ASYNC_STATE_MACHINE_H
#define ASYNC_STATE_MACHINE_H

#include <memory>
#include <set>
#include <chrono>
#include <thread>
#include <functional>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/pointer_cast.hpp>

namespace cblp {

struct async_callback_state_machine : public boost::enable_shared_from_this<async_callback_state_machine>
{
    enum  call_result_type {
        succesed,
        failed,
        aborted,
        timeouted
    };

    typedef boost::function<void (const call_result_type&)> callback_type;

    struct i_task : public boost::enable_shared_from_this<i_task>
    {
        virtual void exec() = 0;
        virtual ~i_task() {}

        boost::weak_ptr<boost::asio::deadline_timer> timer;
    };

    virtual bool start() = 0;

    /// \brief creates state machine, it should be shared_ptr
    static boost::shared_ptr<async_callback_state_machine> create(boost::asio::io_service& io_service,
                                                                   int64_t timeout,
                                                                   async_callback_state_machine::callback_type finalizer);

    virtual ~async_callback_state_machine();

protected:
    async_callback_state_machine(boost::asio::io_service& io_service
                                 , int64_t timeout
                                 , async_callback_state_machine::callback_type finalizer);

    static void deadline_handler(const boost::system::error_code& ec,
                                 boost::shared_ptr<async_callback_state_machine>& machine);


    boost::asio::io_service& io_service;
    boost::asio::io_service::strand strand;
    int64_t timeout_msec;

    boost::shared_ptr<boost::asio::deadline_timer> deadline_timer;
    std::set<boost::shared_ptr<i_task> > scheduled_tasks;
    std::set<boost::shared_ptr<boost::asio::deadline_timer>> active_timers;
    callback_type final_callback;
    std::chrono::high_resolution_clock::time_point timestamp;

public:
    /// \brief schedule task to be executed (immediatelly)
    /// \param task to execute
    void schedule_task(boost::shared_ptr<i_task> task);

    /// \brief schedule task to be executed in "timeout" milliseconds
    /// \param task to execute
    /// \param time interval
    void schedule_task(boost::shared_ptr<i_task> task, int timeout);

    /// \brief deactivate already scheduled task
    /// \param task to deactivate, could be outside the list
    ///        of allready scheduled tasks
    void unschedule_task(boost::weak_ptr<i_task> task);

    /// \brief stop machine, deactivate all tasks, stops all timers
    void stop(call_result_type result = call_result_type::aborted);

private:
    struct weak_binder final
    {
        weak_binder(boost::shared_ptr<i_task>& task
                    , boost::shared_ptr<async_callback_state_machine> machine
                    = boost::shared_ptr<async_callback_state_machine>() )
            : data(task)
            , machine(machine)
        {}

        void operator ()() {
            if ( boost::shared_ptr<i_task> ptr = data.lock() ) {
                ptr->exec();
                if (boost::shared_ptr<async_callback_state_machine> tmp =  machine.lock())
                    tmp->remove_scheduled_task(ptr);
            }
        }
        boost::weak_ptr<i_task> data;
        boost::weak_ptr<async_callback_state_machine> machine;
    };

    struct timer_binder final
    {
        timer_binder(boost::shared_ptr<i_task>& task,
                     boost::shared_ptr<async_callback_state_machine> machine,
                     int64_t timeout_msec)
            : data(task)
            , machine(machine)
            , timer(new boost::asio::deadline_timer(machine->io_service,
                                                    boost::posix_time::milliseconds(timeout_msec)))
        {
            boost::shared_ptr<i_task> tmp = this->data.lock();
            if (tmp)
                tmp->timer = timer;
        }

        void operator ()() {
            if ( boost::shared_ptr<i_task> ptr = data.lock() ) {
                ptr->exec();
                if (boost::shared_ptr<async_callback_state_machine> tmp =  machine.lock())
                    tmp->remove_scheduled_task(ptr);
            }
        }

        static void timeout_handler(const boost::system::error_code& ec,
                                    boost::shared_ptr<timer_binder>& binder)
        {
            if ( ec == boost::asio::error::operation_aborted )
                return;
            (*binder)();
        }

        boost::weak_ptr<i_task> data;
        boost::weak_ptr<async_callback_state_machine> machine;
        boost::shared_ptr<boost::asio::deadline_timer> timer;
    };

    friend struct weak_binder;
    friend struct timer_binder;

    void remove_scheduled_task(boost::shared_ptr<i_task>& ptr);

    boost::recursive_mutex _mutex;
};


} // namespace cblp

#endif //  ASYNC_STATE_MACHINE_H
