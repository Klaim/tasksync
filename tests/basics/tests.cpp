#include <gtest/gtest.h>

#include <future>
#include <atomic>
#include <string>
#include <type_traits>

#include <netrush/system/core/tasksynchronizer.hpp>
#include <netrush/system/core/task.hpp>

using namespace netrush::core;


TEST( Test_TaskSynchronizer, sync_type_never_move )
{
    static_assert( std::is_copy_constructible<TaskSynchronizer>::value == false, "" );
    static_assert( std::is_copy_assignable<TaskSynchronizer>::value == false, "" );
    static_assert( std::is_move_constructible<TaskSynchronizer>::value == false, "" );
    static_assert( std::is_move_assignable<TaskSynchronizer>::value == false, "" );

}


TEST( Test_TaskSynchronizer, namable )
{
    static const auto name = "any_name";
    TaskSynchronizer task_sync{ name };
    EXPECT_EQ( name, task_sync.name() );
}


TEST( Test_TaskSynchronizer, no_task_no_problem)
{
    TaskSynchronizer task_sync;
    task_sync.join_tasks();
}

TEST( Test_TaskSynchronizer, is_joined_after_join_not_after_reset )
{
    TaskSynchronizer task_sync;

    EXPECT_FALSE( task_sync.is_joined() );

    task_sync.join_tasks();

    EXPECT_TRUE( task_sync.is_joined() );

    task_sync.reset();

    EXPECT_FALSE( task_sync.is_joined() );

    task_sync.join_tasks();

    EXPECT_TRUE( task_sync.is_joined() );
}


TEST( Test_TaskSynchronizer, once_joined_no_op )
{
    TaskSynchronizer task_sync;
    task_sync.join_tasks();
    EXPECT_TRUE( task_sync.is_joined() );

    task_sync.join_tasks(); // nothing happen if we call it twice
    EXPECT_TRUE( task_sync.is_joined() );

    auto no_op = task_sync.synchronized( [] { ADD_FAILURE(); } );
    no_op();
}


TEST( Test_TaskSynchronizer, unexecuted_synched_task_never_block_join )
{
    TaskSynchronizer task_sync;
    auto synched_task = task_sync.synchronized( [] { ADD_FAILURE(); } );
    task_sync.join_tasks();
    synched_task();
}

TEST( Test_TaskSynchronizer, finished_synched_task_never_block_join )
{
    int execution_count = 0;
    TaskSynchronizer task_sync;
    auto synched_task = task_sync.synchronized( [&] { ++execution_count; } );
    EXPECT_EQ( 0, execution_count );
    synched_task();
    EXPECT_EQ( 1, execution_count );

    task_sync.join_tasks();

    EXPECT_EQ( 1, execution_count );
    synched_task();
    EXPECT_EQ( 1, execution_count );

}


TEST( Test_TaskSynchronizer, executed_synched_task_never_block_join )
{
    int execution_count = 0;
    TaskSynchronizer task_sync;

    auto synched_task = task_sync.synchronized( [&] { ++execution_count; } );
    auto task_future = std::async( synched_task );
    task_future.wait();

    task_sync.join_tasks();

    synched_task();

    EXPECT_EQ( 1, execution_count );
}

TEST( Test_TaskSynchronizer, executing_synched_task_always_block_join )
{
    using namespace netrush;

    std::string sequence;
    TaskSynchronizer task_sync;

    std::atomic<bool> end_task_requested{ false };

    sequence.push_back( 'A' );

    auto synched_task = task_sync.synchronized( [&] {
        sequence.push_back( 'B' );
        while( !end_task_requested )
        {
            this_thread::sleep_for( milliseconds( 1 ) );
        }
        sequence.push_back( 'D' );
    } );
    auto bs_future = std::async( synched_task );

    this_thread::sleep_for( milliseconds( 100 ) );

    auto bs_future_again = std::async( [&] {
        this_thread::sleep_for( milliseconds( 300 ) );
        sequence.push_back( 'C' );
        end_task_requested = true;
    } );

    task_sync.join_tasks();
    sequence.push_back( 'E' );

    EXPECT_EQ( "ABCDE", sequence );

}


TEST( Test_TaskSynchronizer, throwing_task_never_block_join )
{
    TaskSynchronizer task_sync;

    auto synched_task = task_sync.synchronized( [] { throw 42; } );
    auto task_future = std::async( synched_task );
    task_future.wait();

    task_sync.join_tasks();

    synched_task();

    EXPECT_THROW( task_future.get(), int );
}


