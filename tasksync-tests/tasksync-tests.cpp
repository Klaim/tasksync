#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <atomic>
#include <string>
#include <type_traits>

#include <tasksync/tasksync.hpp>

using namespace tasksync;


TEST_CASE( "sync types never move" )
{
    static_assert( std::is_copy_constructible<TaskSynchronizer>::value == false, "" );
    static_assert( std::is_copy_assignable<TaskSynchronizer>::value == false, "" );
    static_assert( std::is_move_constructible<TaskSynchronizer>::value == false, "" );
    static_assert( std::is_move_assignable<TaskSynchronizer>::value == false, "" );

}


TEST_CASE( "no task no problem" )
{
    TaskSynchronizer task_sync;
    task_sync.join_tasks();
}

TEST_CASE( "tasks are joined after join not after reset" )
{
    TaskSynchronizer task_sync;
    CHECK( !task_sync.is_joined() );

    task_sync.join_tasks();
    CHECK( task_sync.is_joined() );

    task_sync.reset();
    CHECK( !task_sync.is_joined() );

    task_sync.join_tasks();
    CHECK( task_sync.is_joined() );
}

namespace {
    void fail_now()
    {
        throw "this code should never be executed";
    }
}

TEST_CASE( "once joined tasks are no-op" )
{
    TaskSynchronizer task_sync;
    task_sync.join_tasks();
    CHECK( task_sync.is_joined() );

    task_sync.join_tasks(); // nothing happen if we call it twice
    CHECK( task_sync.is_joined() );

    auto no_op = task_sync.synchronized( [] { fail_now(); } );
    no_op();
}


TEST_CASE( "unexecuted synched task never blocks join" )
{
    TaskSynchronizer task_sync;
    auto synched_task = task_sync.synchronized( [] { fail_now(); } );
    task_sync.join_tasks();
    synched_task();
}

TEST_CASE( "finished synched task never blocks join" )
{
    int execution_count = 0;
    TaskSynchronizer task_sync;
    auto synched_task = task_sync.synchronized( [&] { ++execution_count; } );
    CHECK( execution_count == 0 );

    synched_task();
    CHECK( execution_count == 1 );

    task_sync.join_tasks();
    CHECK( execution_count == 1 );

    synched_task();
    CHECK( execution_count == 1 );

}


TEST_CASE( "executed synched task never blocks join" )
{
    int execution_count = 0;
    TaskSynchronizer task_sync;

    auto synched_task = task_sync.synchronized( [&] { ++execution_count; } );
    auto task_future = std::async( std::launch::async, synched_task );
    task_future.wait();

    task_sync.join_tasks();

    synched_task();

    CHECK( execution_count == 1 );
}

namespace {
    template<typename Predicate>
    void wait_condition(Predicate&& predicate)
    {
        while(!std::invoke(std::forward<Predicate>(predicate)))
            std::this_thread::yield();
    }
}

TEST_CASE( "executing synched task always block join")
{
    std::string sequence;
    TaskSynchronizer task_sync;

    const auto unlock_duration = std::chrono::seconds{1};
    std::atomic<bool> task_started{ false };
    std::atomic<bool> task_continue{ false };
    std::atomic<bool> unlocker_ready{ false };
    std::atomic<bool> unlocker_start{ false };

    auto ft_task = std::async( std::launch::async, task_sync.synchronized([&]{
        sequence.push_back('A');
        task_started = true;
        wait_condition([&]{ return task_continue.load(); });
        sequence.push_back('F');
    }));

    wait_condition([&]{ return task_started.load(); });
    CHECK( sequence == "A"  );

    auto ft_unlocker = std::async( std::launch::async, task_sync.synchronized([&]{
        sequence.push_back('B');
        unlocker_ready = true;
        wait_condition([&]{ return unlocker_start.load(); });
        sequence.push_back('D');
        std::this_thread::sleep_for(unlock_duration); // Make sure the time is long enough for joining to happen only after.
        sequence.push_back('E');
        task_continue = true;
    }));

    wait_condition([&]{ return unlocker_ready.load(); });
    CHECK( sequence == "AB" );

    sequence.push_back('C');

    const auto begin_time = std::chrono::high_resolution_clock::now();
    std::atomic_signal_fence(std::memory_order_acq_rel); // prevents the compiler from reordering

    unlocker_start = true;
    task_sync.join_tasks();

    std::atomic_signal_fence(std::memory_order_acq_rel); // prevents the compiler from reordering
    const auto end_time = std::chrono::high_resolution_clock::now();

    CHECK( sequence == "ABCDEF"  );
    CHECK( end_time - begin_time >= unlock_duration );

}


TEST_CASE( "throwing task never block join" )
{
    TaskSynchronizer task_sync;

    auto synched_task = task_sync.synchronized( [] { throw 42; } );
    auto task_future = std::async( std::launch::async, synched_task );
    task_future.wait();

    task_sync.join_tasks();

    synched_task();

    CHECK_THROWS_AS( task_future.get(), int );
}


