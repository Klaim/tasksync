#pragma once

#include <atomic>
#include <string>
#include <boost/scope_exit.hpp>

#include <utilcxx/assert.hpp>

#include "thread.hpp"
#include "trycall.hpp"
#include "log.hpp"
#include "stringview.hpp"

namespace netrush {
namespace core {

    /** Synchronize tasks execution in multiple threads with it's lifetime.

        Any callable object can be transformed to another similar callable object
        that will synchronize it's execution with this synchronizer.

        Once one of this synchronizer's joining function is called,
        synchronized callables will behave as follow:
            - if a callable was never called before, it's body will never be executed
                when called after the joining call;
            - if the callable is being called, the joining function will block until
                it's execution is finished.

        Also provides synchronization with ReschedulableTask objects to stop rescheduling once
        a joining function is called, in addition to the behaviour described before.


    */
    class TaskSynchronizer
    {
    public:

        /** Construction with name.

            The name is used in debugging logs to help with debugging released versions.

            @param name     Pointer to characters of the name that must be null-terminated
                            and must outlive this object's lifetime.
        */
        explicit TaskSynchronizer( string_view name )
            : m_name( name )
        {
        }

        /** Default constructor, no name provided on logging. */
        TaskSynchronizer()
            : m_name( "<unnamed>" )
        {
        }

        /** Destructor, joining tasks synchronized with this object.
            @see join_tasks()
        */
        ~TaskSynchronizer()
        {
            join_tasks();
        }

        TaskSynchronizer( const TaskSynchronizer& ) = delete;
        TaskSynchronizer& operator=( const TaskSynchronizer& ) = delete;

        TaskSynchronizer( TaskSynchronizer&& other ) noexcept = delete;
        TaskSynchronizer& operator=( TaskSynchronizer&& other ) noexcept = delete;

        /** Transform the provided callable into a similar but synchronized callable.

            Wraps the body of the callable into another callable which will, on call,
            checks that:
                - if joining function of this synchronizer have been called, skip execution;
                - if no joining function have been called, notify the synchronizer that the
                    execution begins, then execute the body;

            Note that if the callable is a ReschedulableTask, it will not be unscheduled after join.
            To achieve that, use the task synchronization make_synchronized() instead.

            @remark Throws an exception if called while is_joined() is true.

            @param work     Any callable object.
            @return A wrapped version of the provided callable object, adding checks
                preventing execution of the original callable body if any joining function
                of this synchronizer was called.
        */
        template< class Work >
        auto synchronized( Work&& work )
        {
            return [ this, new_work = std::forward<Work>( work ), remote_status = make_remote_status() ]
            ( auto&&... args ) mutable
            {
                // If status is alive then we know the TaskSynchronizer is alive too.
                auto status = remote_status.lock();
                if( status && !status->join_requested ) // Don't add running tasks while join was requested.
                { // We can use 'this' safely in this scope.
                    notify_begin_execution();
                    BOOST_SCOPE_EXIT_ALL(this){ notify_end_execution(); };
                    try_call( new_work, std::forward<decltype( args )>( args )... );
                    status.reset(); // Make sure we are not keeping the TaskSynchronizer waiting
                }
            };
        }

        /** Create a synchronized object of type meeting the ReschedulableTask concept.
            @see Task as one compatible type meeting the ReschedulableTask requirements.

            @tparam TaskType    A type matching the ReschedulableTask concept.
            @param init_args    Initialization arguments passed to the object's constructor.
            @return A TaskType object constructed with the initialization arguments and setup
                    to be in sync with this synchronizer.
        */
        template< class TaskType, class... InitArgs >
        TaskType make_synchronized( InitArgs&&... init_args )
        {
            return TaskType( std::forward<InitArgs>( init_args )... )
                .until( [ remote_status = make_remote_status() ]{
                    return remote_status.expired();
                });
        }

        /** Notify all synchronized tasks and blocks until all executing synchronized tasks are done.

            This is a joining function: once it is called, no synchronized task body will be executed again.
            Synchronized tasks which body is being executed will notify this synchronizer once done.

            Only returns once all the executing tasks have finished finishes.

            After calling this, is_joined() will return true.
        */
        void join_tasks()
        {
            NR_LOG_TRACE( "Joining tasks synched with TaskSynchronizer '" << m_name << "'..." );
            wait_all_running_tasks();
            UCX_ASSERT_TRUE( is_joined() );
            NR_LOG_TRACE( "Joining tasks synched with TaskSynchronizer '" << m_name << "' - DONE" );
        }

        /** Join synchronized tasks and reset this object's state to be reusable like if it was just constructed.

            Similar to calling join_tasks() but is_joined() will return false after calling this.

            @see join_tasks()
        */
        void reset()
        {
            join_tasks();
            m_status = std::make_shared<Status>();
            UCX_ASSERT_FALSE( is_joined() );
        }

        /** @return Name of this object that will be used in logging if set on construction. */
        string_view name() const { return m_name; }

        /** @return true if all synchronized tasks have beeen joined, false otherwise. @see join_tasks(), reset()*/
        bool is_joined() const { return !m_status && m_running_tasks == 0; }

        /** @return Number of synchronized tasks which are currently beeing executed. */
        int64_t running_tasks() const { return m_running_tasks; }

    private:

        struct Status
        {
            std::atomic<bool> join_requested { false };
        };

        const string_view m_name;
        std::atomic<int64_t> m_running_tasks{ 0 };

        std::shared_ptr<Status> m_status = std::make_shared<Status>();

        mutex m_mutex;
        condition_variable m_task_end_condition;

        void notify_begin_execution()
        {
            ++m_running_tasks;
        }

        void notify_end_execution()
        {
            unique_lock<mutex> exit_lock{ m_mutex };
            --m_running_tasks;
            m_task_end_condition.notify_one();
        }

        auto make_remote_status()
        {
            return std::weak_ptr<Status>{ m_status };
        }

        void wait_all_running_tasks()
        {
            if( !m_status )
                return;

            unique_lock<mutex> exit_lock{ m_mutex };

            auto remote_status = make_remote_status();
            m_status->join_requested = true;
            m_status = {};

            m_task_end_condition.wait( exit_lock, [this, remote_status] {
                return m_running_tasks == 0
                    && remote_status.expired();
            } );


        }


    };

}}


