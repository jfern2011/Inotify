/**
 *  \file   Signal.h
 *  \author Jason Fernandez
 *  \date   10/13/2017
 *
 *  https://github.com/jfern2011/signal
 */

#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <cstddef>
#include <tuple>
#include <utility>

namespace Signal
{

#ifndef DOXYGEN_SKIP
    /*
     * Utilities for generating integer sequences used for expanding
     * arguments to signal handlers:
     *
     * http://stackoverflow.com/questions/7858817/
     *         unpacking-a-tuple-to-call-a-matching-function-pointer
     */
    template<int...>
    struct seq
    {
    };

    template<int N, int... S>
    struct gens : gens<N-1, N-1, S...>
    {
    };

    template<int... S>
    struct gens<0, S...>
    {
        typedef seq<S...> type;
    };

    /*
     * A container for bound function arguments:
     */
    template <class... T>
    struct SignalArgs
    {
        std::tuple<typename std::remove_const<
                   typename std::remove_reference<T>::type>::type...>
            args;
    };
#endif

    /**
     ******************************************************************
     *
     * @class generic
     *
     * Allows for invoking signal handlers through a single (abstract)
     * class, regardless of signal type
     *
     ******************************************************************
     */
    class generic
    {

    public:

        virtual ~generic() {}

        virtual generic* clone()    const = 0;
        virtual bool is_connected() const = 0;
        virtual void v_raise() = 0;
    };

    /**
     ******************************************************************
     *
     * @class signal_t
     *
     * An interface that can be used to access your signal when it is
     * either a fcn_ptr or mem_ptr
     *
     * @tparam R  Signal handler return type
     * @tparam A  Specifies the type(s) of each input argument to the
     *            signal handler
     *
     ******************************************************************
     */
    template <class R, class... A>
    class signal_t : public generic
    {

    public:

        virtual ~signal_t() {}

        virtual generic* clone()    const = 0;
        virtual bool detach() = 0;
        virtual R raise(A...) = 0;
    };

    /**
     ******************************************************************
     *
     * @class mem_ptr
     *
     * Represents a signal whose handler is a member of a particular
     * class
     *
     * @tparam R  The signal handler return type
     * @tparam C  Class that implements the handler
     * @tparam A  Specifies the type(s) of the arguments required by
     *            the handler
     *
     ******************************************************************
     */
    template <class R, class C, class... A>
    class mem_ptr : public signal_t<R,A...>
    {
        using Handler       = R(C::*)(A...);
        using const_Handler = R(C::*)(A...) const;

    public:

        /**
         * Constructor (1)
         *
         * @param [in] obj The object through which to invoke the signal
         *                 handler
         */
        mem_ptr(C& obj)
            : _const_func(nullptr), _func(nullptr), _is_init(false), _obj(obj)
        {
        }

        /**
         * Constructor (2)
         *
         * @param[in] obj  The object through which to invoke the signal
         *                 handler
         * @param[in] func A pointer to the handler which is a member of
         *                 class C
         */
        mem_ptr(C& obj, const Handler func)
            : _const_func(nullptr), _func(func), _obj(obj)
        {
            _is_init = _func != nullptr;
        }

        /**
         * Constructor (3)
         *
         * @param[in] obj  The object through which to invoke the signal
         *                 handler
         * @param[in] func A pointer to the *const* signal handler which
         *                 is a member of class C
         */
        mem_ptr(C& obj, const const_Handler func)
            : _const_func(func), _func(nullptr), _obj(obj)
        {
            _is_init = _const_func != nullptr;
        }

        /**
         * Destructor
         */
        ~mem_ptr()
        {
        }

        /**
         * Attach a new signal handler
         *
         * @param[in] func A pointer to the handler which is a member of
         *                 class C
         *
         * @return True on success
         */
        bool attach(const Handler func)
        {
            if (func == nullptr)
                return false;

            _func = func;
            _const_func = nullptr;

            _is_init = true;
            return true;
        }

        /**
         * Attach a new signal handler
         *
         * @param[in] func A pointer to the *const* signal handler which
         *                 is a member of class C
         *
         * @return True on success
         */
        bool attach(const const_Handler func)
        {
            if (func == nullptr)
                return false;

            _func = nullptr;
            _const_func = func;

            _is_init = true;
            return true;
        }

        /**
         * Bind arguments to the signal handler
         *
         * @param [in] args A set of arguments to implicitly pass to
         *                  the handler
         */
        template <typename... T>
        void bind(T&&... args)
        {
            _sargs.args = std::make_tuple(std::forward<T>(args)...);
        }

        /**
         * A factory method that creates a copy of this object
         *
         * @return A \ref generic pointer to the newly created
         *         object
         */
        generic* clone() const
        {
            mem_ptr<R,C,A...>* sig = new mem_ptr<R,C,A...>(_obj);

            sig->_const_func = _const_func;
            sig->_func       = _func;
            sig->_is_init    = _is_init;
            sig->_sargs      = _sargs;

            return sig;
        }

        /**
         * Detach the current signal handler
         *
         * @return True on success
         */
        bool detach()
        {
            _is_init = false;
            _const_func = nullptr; _func = nullptr;

            return true;
        }

        /**
         * Determine if this signal is currently attached via \ref
         * attach()
         *
         * @return True if attached
         */
        bool is_connected() const
        {
            return !(_const_func == nullptr && _func == nullptr);
        }

        /**
         * Invoke the signal handler. This will fail if no handler is
         * attached, which can be verified with is_connected()
         *
         * @param[in] args The input arguments to provide the handler
         *                 with
         *
         * @return The return value of the handler
         */
        R raise(A... args)
        {
            if (_func != nullptr)
                return (_obj.*_func)(std::forward<A>(args)...);
            else
                return
                 (_obj.*_const_func)(std::forward<A>(args)...);
        }

        /**
         * Forward bound arguments to the signal handler. Note this
         * will fail if no handler is currently attached, which can
         * be verified with is_connected()
         *
         * @return The return value of the handler
         */
        template <int N=0>
        R raise()
        {
            return
                run(typename gens<sizeof...(A)>::type());
        }

        /**
         * Forward bound arguments to the signal handler. Note this
         * will fail if no handler is currently attached, which can
         * be verified with is_connected()
         *
         * This is equivalent to \ref raise() except that it can be
         * invoked through a \ref generic
         */
        void v_raise()
        {
            raise();
        }

    protected:

        template<int... S>
        R run(seq<S...>)
        {
            if (_func != nullptr)
                return (_obj.*_func)(std::get<S>( _sargs.args)...);
            else
                return
                 (_obj.*_const_func)(std::get<S>( _sargs.args)...);
        }

        const_Handler
                _const_func;
        Handler _func;
        bool    _is_init;
        C&      _obj;

        SignalArgs< A... >
                _sargs;
    };

    /**
     ******************************************************************
     *
     * @class fcn_ptr
     *
     * Represents a signal whose handler is a C-style function pointer
     *
     * @tparam R  Signal handler return type
     * @tparam A  Specifies the type(s) of input arguments required by
     *            the handler
     *
     ******************************************************************
     */
    template <class R, class... A>
    class fcn_ptr : public signal_t<R,A...>
    {
        using Handler = R(*)(A...);

    public:

        /**
         * Default constructor
         */
        fcn_ptr()
            : _func(nullptr), _is_init(false)
        {
        }

        /**
         * Construct a fcn_ptr from a handler
         *
         * @param[in] func The handler, which is a C-style function
         *                 pointer
         */
        fcn_ptr(const Handler func)
            : _func(func)
        {
            _is_init = _func != nullptr;
        }

        /**
         * Destructor
         */
        ~fcn_ptr()
        {
        }

        /**
         * Attach a new signal handler
         *
         * @param[in] func The handler, which is a C-style function
         *                 pointer
         *
         * @return True on success
         */
        bool attach(const Handler func)
        {
            if (func == nullptr)
                return false;

            _func = func;
            _is_init = true;

            return true;
        }

        /**
         * Bind arguments to the signal handler. This avoids having
         * to call raise() with explicit inputs
         *
         * @param[in] args Input arguments to implicitly forward
         */
        template <typename... T>
        void bind(T&&... args)
        {
            _sargs.args = std::make_tuple(std::forward<T>(args)...);
        }

        /**
         * A factory method that creates a copy of this object
         *
         * @return A \ref generic pointer to the newly created
         *         object
         */
        generic* clone() const
        {
            fcn_ptr<R,A...>* sig = new fcn_ptr<R,A...>( _func );

            sig->_is_init    = _is_init;
            sig->_sargs      = _sargs;

            return sig;
        }

        /**
         * Detach the current signal handler
         *
         * @return True on success
         */
        bool detach()
        {
            _is_init = false; _func = nullptr;

            return true;
        }

        /**
         * Determine if this signal is currently attached
         *
         * @return True if attached
         */
        bool is_connected() const
        {
            return _func != nullptr;
        }

        /**
         * Invoke the signal handler. This will fail if no handler is
         * attached, which can be verified with is_connected()
         *
         * @param[in] args The input arguments to provide the handler
         *                 with
         *
         * @return The return value of the handler
         */
        R raise(A... args)
        {
            return _func(std::forward<A>(args)...);
        }

        /**
         * Forward bound arguments to the signal handler. Note this
         * will fail if no handler is currently attached, which can
         * be verified with is_connected()
         *
         * @return The return value of the handler
         */
        template <int N=0>
        R raise()
        {
            return
                run(typename gens<sizeof...(A)>::type());
        }

        /**
         * Forward bound arguments to the signal handler. Note this
         * will fail if no handler is currently attached, which can
         * be verified with is_connected()
         *
         * This is equivalent to \ref raise() except that it can be
         * invoked through a \ref generic
         */
        void v_raise()
        {
            raise();
        }

    protected:

        template<int... S>
        R run(seq<S...>)
        {
            return _func (std::get<S>( _sargs.args)... );
        }

        Handler _func;
        bool    _is_init;

        SignalArgs<A...>
                _sargs;
    };

    /**
     ******************************************************************
     *
     * @class Signal
     *
     * Enables the creation of customized "signals" that trigger events
     * of interest which are application-specific. Each Signal wraps
     * a generic function pointer or class method to be used as a
     * handler when the Signal is raised, similar to using the standard
     * signal library
     *
     * @tparam R  The signal handler's return type
     * @tparam A  Specifies the type(s) of input arguments required by
     *            the handler
     *
     ******************************************************************
     */
    template <class R, class... A>
    class Signal : public generic
    {
    public:

        /**
         * Default constructor
         */
        Signal()
            : _is_mem_ptr(false), _sig(nullptr)
        {
        }

        /**
         * Create a signal with a handler
         *
         * @param[in] func A pointer to the signal handler
         */
        Signal(R(*func)(A...))
            : _is_mem_ptr(false)
        {
            _sig = new fcn_ptr<R,A...>(func);
        }

        /**
         * Create a signal whose handler is a member function of class C
         *
         * @tparam C Class that implements the handler 
         *
         * @param[in] obj  Object (of class C) through which to invoke
         *                 the handler
         * @param[in] func A pointer to the signal handler which belongs
         *                 to class C
         */
        template <typename C>
        Signal(C& obj, R(C::*func)(A...))
            : _is_mem_ptr(true)
        {
            _sig = new mem_ptr<R,C,A...>(obj, func);
        }

        /**
         * Create a signal whose handler is a member function of class C
         *
         * @tparam C Class that implements the handler 
         *
         * @param[in] obj  Object (of class C) through which to invoke
         *                 the handler
         * @param[in] func A pointer to the *const* signal handler which
         *                 belongs to class C
         */
        template <typename C>
        Signal(C& obj, R(C::*func)(A...) const)
            : _is_mem_ptr(true)
        {
            _sig = new mem_ptr<R,C,A...>(obj, func);
        }

        /**
         * Copy constructor
         *
         * @param [in] other The Signal of which *this will be a copy
         */
        Signal(const Signal<R,A...>& other)
            : _sig(nullptr)
        {
            *this = other;
        }

        /**
         * Move constructor
         *
         * @param [in] other A Signal to move into *this. This leaves
         *                   \a other detached
         */
        Signal(Signal<R,A...>&& other)
            : _sig(nullptr)
        {
            *this = std::move( other );
        }

        /**
         * Destructor
         */
        ~Signal()
        {
            if (_sig) delete _sig;
        }

        /**
         * Copy assignment operator
         *
         * @param[in] rhs Another Signal to create a copy of
         *
         * @return *this
         */
        Signal<R,A...>& operator=(const Signal<R,A...>& rhs)
        {
            if (this != &rhs)
            {
                if (_sig) delete _sig;

                _is_mem_ptr = rhs._is_mem_ptr;
                _sargs = rhs._sargs;
                _sig   = dynamic_cast<
                    signal_t<R,A...>*>( rhs._sig->clone() );
            }

            return *this;
        }

        /**
         * Move assignment operator
         *
         * @param [in] rhs A Signal to move into *this. This leaves
         *                 \a rhs detached
         *
         * @return *this
         */
        Signal<R,A...>& operator=(Signal<R,A...>&& rhs)
        {
            if (this != &rhs)
            {
                if (_sig) delete _sig;
                
                _is_mem_ptr = rhs._is_mem_ptr;
                _sargs      = std::move ( rhs._sargs );
                _sig = rhs._sig;
                
                rhs._is_mem_ptr = false;
                rhs._sig = nullptr;
            }

            return *this;
        }

        /**
         * Attach a handler to this Signal, removing the previous
         * handler (if it exists)
         *
         * @param[in] func A pointer to the signal handler
         *
         * @return  True if the handler was successfully attached
         */
        bool attach(R(*func)(A...))
        {
            if (is_connected() && !detach())
                return false;

            _sig = new fcn_ptr<R,A...>(func);
            _is_mem_ptr = false;

            return _sig->is_connected();
        }

        /**
         * Attach a handler to this Signal, removing the previous
         * handler (if it exists)
         *
         * @tparam C Class that implements the handler 
         *
         * @param[in] obj  Object (of class C) through which to
         *                 invoke the handler
         * @param[in] func A pointer to the signal handler
         *
         * @return  True if the handler was successfully attached
         */
        template <typename C>
        bool attach(C& obj, R(C::*func)(A...))
        {
            if (is_connected() && !detach())
                return false;
            else
            {
                _sig = new mem_ptr<R,C,A...>(obj, func);
                _is_mem_ptr = true;
            }

            return _sig->is_connected();
        }

        /**
         * Associate a new handler with this Signal without changing 
         * the object through which it is called. This method will
         * fail if this signal was not initialized with an object of
         * class C
         *
         * @tparam C Class that implements the signal handler 
         *
         * @param[in] func A pointer to the handler
         *
         * @return True if the new handler was successfully attached
         */
        template <typename C>
        bool attach(R(C::*func)(A...))
        {
            if (!_is_mem_ptr || !is_connected())
                return false;
            else
                dynamic_cast<mem_ptr<R,C,A...>*>(_sig)->attach(func);

            return _sig->is_connected();
        }

        /**
         * Attach a handler to this Signal, removing the previous
         * handler (if it exists)
         *
         * @tparam C Class that implements the handler 
         *
         * @param[in] obj  Object (of class C) through which to
         *                 invoke the handler
         * @param[in] func A pointer to the *const* handler
         *
         * @return True if the handler was successfully attached
         */
        template <typename C>
        bool attach(C& obj, R(C::*func)(A...) const)
        {
            if (is_connected() && !detach())
                return false;
            else
            {
                _sig = new mem_ptr<R,C,A...>(obj, func);
                _is_mem_ptr = true;
            }

            return _sig->is_connected();
        }

        /**
         * Associate a new handler with this Signal without changing 
         * the object through which it is called. This method will
         * fail if this signal was not initialized with an object of
         * class C
         *
         * @tparam C The class that implements the signal handler 
         *
         * @param[in] func A pointer to the *const* handler
         *
         * @return True if the new handler was successfully attached
         */
        template <typename C>
        bool attach(R(C::*func)(A...) const)
        {
            if (!_is_mem_ptr || !is_connected())
                return false;
            else
                dynamic_cast<mem_ptr<R,C,A...>*>(_sig)->attach(func);

            return _sig->is_connected();
        }

        /**
         * Bind arguments to the signal handler
         *
         * @param[in] args The set of arguments to implicitly pass to
         *                 the handler
         */
        template <typename... T>
        void bind(T&&... args)
        {
            _sargs.args =
                std::make_tuple(std::forward<T>(args)...);
        }

        /**
         * A factory method that creates a copy of this object
         *
         * @return A \ref generic pointer to the newly created
         *         object
         */
        virtual generic* clone() const
        {
            return new Signal(*this);
        }

        /**
         * Detach a signal handler
         *
         * @return True on success, or false if no handler is attached
         */
        bool detach()
        {
            if (!_sig) return false;

            delete _sig; _sig = nullptr;
            _is_mem_ptr = false;

            return true;
        }

        /**
         * Check whether a handler is attached to this Signal
         *
         * @return True if a handler is currently attached
         */
        bool is_connected() const
        {
            if (!_sig)
                return false;

            return _sig->is_connected();
        }

        /**
         * Forward a set of arguments to the signal handler. This will
         * fail if a handler is not attached. Use is_connected() to
         * determine if a signal can safely be raised. Calling this
         * on a detached signal will raise a seg fault (which probably
         * isn't the signal you intended)
         *
         * @param [in] args The input arguments to provide the handler
         *                  with
         * 
         * @return The handler's return value
         */
        R raise(A... args)
        {
            return _sig->raise(std::forward<A>(args)...);
        }

        /**
         * Forward bound arguments to the signal handler. Note this
         * will fail if no handler is currently attached, which can
         * be verified with is_connected()
         *
         * @return The return value of the handler
         */
        template <int N=0>
        R raise()
        {
            return
                run(typename gens<sizeof...(A)>::type());
        }

        /**
         * Forward bound arguments to the signal handler. Note this
         * will fail if no handler is currently attached, which can
         * be verified with is_connected()
         *
         * This is equivalent to \ref raise() except that it can be
         * invoked through a \ref generic
         */
        void v_raise()
        {
            raise();
        }

        /*
        Signal(const Signal<R,A...>& rhs)        = delete;
        Signal<R,A...>&
            operator=(const Signal<R,A...>& rhs) = delete;
         */

    private:

        template<int... S>
        R run(seq<S...>)
        {
            return _sig->raise(std::get<S>( _sargs.args)... );
        }

        bool    _is_mem_ptr;

        SignalArgs<A...> 
                _sargs;

        signal_t<R,A...>*
                _sig;
    };

    /**
     ******************************************************************
     *
     * @class Callable
     *
     * Works similarly to the Signal class, except that the handler is
     * a generic callable object
     *
     * @tparam Func The signal handler
     *
     ******************************************************************
     */
    template <class Func>
    class Callable
    {
    public:

        /**
         * Default constructor
         */
        Callable()
            : _is_init(false)
        {
        }

        /**
         * Constructor
         *
         * @param [in] handler A callable object to be used as a signal
         *                     handler
         */
        Callable(Func handler)
            : _handler(handler), _is_init(true)
        {
        }

        /**
         * Destructor
         */
        ~Callable()
        {
        }

        /**
         * Attach a new signal handler
         *
         * @param [in] handler A callable object to be used as a signal
         *                     handler
         *
         * @return True on success
         */
        bool attach(Func handler)
        {
            _handler = handler; _is_init = true;
            return true;
        }

        /**
         * Detach the current signal handler
         *
         * @return True on success
         */
        bool detach()
        {
            _is_init = false;
            return true;
        }

        /**
         * Check whether a handler is attached to this signal
         *
         * @return True if a handler is currently attached
         */
        bool is_connected() const
        {
            return _is_init;
        }

        /**
         * Invoke the signal handler
         *
         * @param[in] args Arguments to forward to the signal
         *                 handler
         *
         * @return The handler's return value
         */
        template <typename... T>
        auto raise(T&&... args)
            -> decltype(std::declval<Func&>()(std::forward<T>(args)...))
        {
            return _handler(std::forward<T>(args)...);
        }

    private:

        Func _handler;
        bool _is_init;
    };
}

#endif // __SIGNAL_H__
