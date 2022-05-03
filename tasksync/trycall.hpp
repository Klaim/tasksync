#pragma once

#include <utility>

namespace netrush {
namespace core {

    namespace detail
    {
        // The following trick is explained there:  http://stackoverflow.com/questions/24495334/how-to-overload-a-template-function-depending-on-arguments-call-operator-args-o
        template<int I> struct priority : priority<I - 1>{};
        template<> struct priority<0>{};

        using otherwise = priority<0>;

        template<class Callable, class... Args>
        auto try_call( priority<2>, Callable&& callable, Args&&... args )
            -> decltype( callable( std::forward<Args>(args)... ), void() )
        {
            callable( std::forward<Args>(args)... );
        }

        template<class Callable, class... Args>
        auto try_call( priority<1>, Callable&& callable, Args&&... )
            -> decltype( callable(), void() )
        {
            callable();
        }

        template<class NotCallable, class... Args>
        void try_call( otherwise, NotCallable&&, Args&&... )
        {}

        template<class CallableRange, class... Args>
        auto try_call_each( priority<2>, CallableRange&& callable_range, Args&&... args )
            -> decltype( callable_range.begin()->operator()( std::forward<Args>(args)... ), void() )
        {
            for( auto&& callable : callable_range )
                callable( std::forward<Args>(args)... );
        }

        template<class CallableRange, class... Args>
        auto try_call_each( priority<1>, CallableRange&& callable_range, Args&&... )
            -> decltype( callable_range.begin()->operator()(), void() )
        {
            for( auto&& callable : callable_range )
                callable();
        }

        template<class NotCallableRange, class... Args>
        void try_call_each( otherwise, NotCallableRange&&, Args&&... )
        {}

        template<class CallableIndexedRange, class... Args>
        auto try_call_each_indexed( priority<2>, CallableIndexedRange&& callable_indexed_range, Args&&... args )
            -> decltype( callable_indexed_range.begin()->second( std::forward<Args>(args)... ), void() )
        {
            for( auto&& slot : callable_indexed_range )
                slot.second( std::forward<Args>(args)... );
        }

        template<class CallableIndexedRange, class... Args>
        auto try_call_each_indexed( priority<1>, CallableIndexedRange&& callable_indexed_range, Args&&... )
            -> decltype( callable_indexed_range.begin()->second(), void() )
        {
            for( auto&& slot : callable_indexed_range )
                slot.second();
        }

        template<class NotCallableIndexedRange, class... Args>
        void try_call_each_indexed( otherwise, NotCallableIndexedRange&&, Args&&... )
        {}
    }

    /** Try to call the object with the provided arguments or not arguments at all.
        If the object is callable with the provided arguments, it will be called immediately.
        Otherwise, if the object is callable with no arguments at all, it will be called immediately.
        Otherwise, if the object can't do either, it will not be called at all.
    */
    template< class Callable, class... Args >
    void try_call( Callable&& callable, Args&&... args )
    {
        return detail::try_call( detail::priority<2>(), std::forward<Callable>(callable), std::forward<Args>(args)... );
    }

    /** Try to call all the objects in the provided range with the provided arguments or not arguments at all.
        If the object is callable with the provided arguments, it will be called immediately.
        Otherwise, if the object is callable with no arguments at all, it will be called immediately.
        Otherwise, if the object can't do either, it will not be called at all.
    */
    template< class CallableRange, class... Args >
    void try_call_each( CallableRange&& callable_range, Args&&... args )
    {
        return detail::try_call_each( detail::priority<2>(), std::forward<CallableRange>(callable_range), std::forward<Args>(args)... );
    }

    /** Try to call all the objects in the provided indexed range with the provided arguments or not arguments at all.
        If the object is callable with the provided arguments, it will be called immediately.
        Otherwise, if the object is callable with no arguments at all, it will be called immediately.
        Otherwise, if the object can't do either, it will not be called at all.
    */
    template< class CallableIndexedRange, class... Args >
    void try_call_each_indexed( CallableIndexedRange&& callable_indexed_range, Args&&... args )
    {
        return detail::try_call_each_indexed( detail::priority<2>(), std::forward<CallableIndexedRange>(callable_indexed_range), std::forward<Args>(args)... );
    }


}}

