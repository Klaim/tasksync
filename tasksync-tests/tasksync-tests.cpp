#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <future>
#include <atomic>
#include <string>
#include <type_traits>

#include <tasksync/tasksync.hpp>

using namespace netrush::core;


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

TEST_CASE( "executing synched task always block join" )
{
    std::string sequence;
    TaskSynchronizer task_sync;

    std::atomic<bool> end_task_requested{ false };
    std::atomic<bool> ready_to_end{ false };

    sequence.push_back( 'A' );

    auto synched_task = task_sync.synchronized( [&] {
        sequence.push_back( 'B' );
        ready_to_end = true;
        while( !end_task_requested )
        {
            std::this_thread::yield();
        }
        sequence.push_back( 'D' );
    } );
    auto bs_future = std::async( std::launch::async, synched_task );

    std::this_thread::sleep_for( std::chrono::milliseconds{ 100 } );

    auto bs_future_again = std::async( std::launch::async, [&] {
        while( !ready_to_end )
        {
            std::this_thread::yield();
        }
        sequence.push_back( 'C' );
        end_task_requested = true;
    } );

    task_sync.join_tasks();
    sequence.push_back( 'E' );

    CHECK( sequence == "ABCDE"  );

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


