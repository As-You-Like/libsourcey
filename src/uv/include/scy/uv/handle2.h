///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @defgroup uv UV module
///
/// The `uv` module contains C++ wrappers for `libuv`.
///
/// @addtogroup uv
/// @{


#ifndef SCY_UV_Handle2_H
#define SCY_UV_Handle2_H


#include "scy/uv/uvpp.h"
#include "scy/uv/loop.h"
#include "scy/uv/util.h"


namespace scy {
namespace uv {


template <typename T>
class UV_API Handle2;


/// Shared `libuv` handle context.
template<typename T>
struct Context
{
    Handle2<T>* handle = nullptr;
    T* ptr = new T;
    bool initialized = false;
    bool deleted = false;

    Context(Handle2<T>* h)
        : handle(h)
    {
    }

    ~Context()
    {
        if (initialized) {
            uv_close(reinterpret_cast<uv_handle_t*>(ptr), [](uv_handle_t* handle) {
                delete handle;
            });
        }
        else {
            delete ptr;
        }
    }
};


/// Request handle wrapper.
template<typename T, typename R>
struct Request
{
    Handle2<T>* parent;
    std::shared_ptr<Context<T>> ctx;
    R req;
    uv_buf_t buf;

    Request(Handle2<T>* p)
        : parent(p)
        , ctx(p->context())
    {
        req.data = this;
    }

    template<typename F, typename... Args>
    bool invoke(F&& f, Args&&... args)
    {
        int err = std::forward<F>(f)(std::forward<Args>(args)...);
        if (err)
            parent->setError("UV Error", err);
        return !err;
    }
};


/// Wrapper class for managing a `libuv` handle.
///
/// This class manages thehandle during it's lifecycle and
/// safely handles the asynchronous destruction mechanism.
template<typename T>
class UV_API Handle2
{
public:
    Handle2(uv::Loop* loop = uv::defaultLoop())
        : _loop(loop)
        , _context(std::make_shared<Context<T>>(this))
    {
    }

    virtual ~Handle2()
    {
    }

    /// Initialize the handle.
    template<typename F, typename... Args>
    bool init(F&& f, Args&&... args)
    {
        assert(_context);
        assert(!initialized());
        int err = std::forward<F>(f)(loop(), get(), std::forward<Args>(args)...);
        if (err)
            setError("Initialization failed", err);
        else
            _context->initialized = true;
        return !err;
    }

    /// Invoke a method on the handle.
    template<typename F, typename... Args>
    bool invoke(F&& f, Args&&... args)
    {
        int err = std::forward<F>(f)(std::forward<Args>(args)...);
        if (err)
            setError("UV Error", err);
        return !err;
    }

    /// Invoke a method on the handle.
    ///
    /// An exception will be thrown if the invoked method returns an error.
    template<typename F, typename... Args>
    void invokeOrThrow(const std::string& message, F&& f, Args&&... args)
    {
        int err = std::forward<F>(f)(std::forward<Args>(args)...);
        if (err)
            setAndThrowError(message, err);
    }

    /// Close and destroy the handle.
    virtual void close()
    {
        bool trigger = false;
        if (_context) {
            _context->deleted = true;
            trigger = true;
        }
        _context = nullptr;
        if (trigger)
            onClose();
    }

    /// Reference main loop again, once unref'd.
    bool ref()
    {
        if (uv_has_ref(get<uv_handle_t>()))
            return false;

        uv_ref(get<uv_handle_t>());
        return true;
    }

    /// Unreference the main loop after initialized.
    bool unref()
    {
        if (!uv_has_ref(get<uv_handle_t>()))
            return false;

        uv_unref(get<uv_handle_t>());
        return true;
    }

    /// Return true if the handle has been initialized.
    bool initialized() const
    {
        return _context && _context->initialized;
    }

    /// Return true when the handle is active.
    virtual bool active() const
    {
        return get() && uv_is_active(get<uv_handle_t>()) != 0;
    }

    /// Return true if the handle is closing.
    virtual bool closing() const
    {
        return get() && uv_is_closing(get<uv_handle_t>()) != 0;
    }

    /// Return true is the handle has been closed.
    virtual bool closed() const
    {
        return !_context;
    }

    /// Return the error context if any.
    const scy::Error& error() const
    {
        return _error;
    }

    /// Set the error and triggers callbacks.
    virtual void setError(const scy::Error& err)
    {
        // if (_error == err) return;
        assertThread();
        _error = err;
        onError(err);
    }

    /// Set the error and trigger relevant callbacks.
    /// This method can be called inside `libuv` callbacks.
    virtual void setError(const std::string& prefix = "UV Error", int errorno = 0)
    {
        scy::Error err;
        err.errorno = errorno;
        // err.syserr = uv.sys_errno_;
        err.message = formatError(prefix, errorno);
        setError(err);
    }

    /// Set the error and throw an exception.
    /// Should never be called inside `libuv` callbacks.
    virtual void setAndThrowError(const std::string& prefix = "UV Error", int errorno = 0)
    {
        setError(prefix, errorno);
        throwError(prefix, errorno);
    }

    /// Throw an exception if the handle is in error state.
    /// The error message prefix will be updated if provided.
    virtual void throwLastError(const std::string& prefix = "UV Error")
    {
        if (error().any())
            setAndThrowError(prefix, error().errorno);
    }

    /// Set the event loop.
    /// The event loop may be set before the handle is initialized.
    // virtual void setLoop(uv::Loop* loop)
    // {
    //     assertThread();
    //     assert(get() == nullptr && "must be set before handle");
    //     _loop = loop;
    // }

    /// Return a cast pointer to the managed `libuv` handle.
    virtual uv::Loop* loop() const
    {
        assertThread();
        return _loop;
    }

    /// Return a pointer to the current or derived instance.
    virtual void* self()
    {
        return this;
    }

    /// Reset the internal handle pointer and container state.
    void reset()
    {
        if (_context)
            close();
        _context.reset(new Context<T>(this));
    }

    /// Return a typecasted pointer to the managed handle.
    template <typename Handle = T>
    Handle* get() const
    {
        assertThread();
        return _context ? reinterpret_cast<Handle*>(_context->ptr) : nullptr;
    }

    /// Return the parent thread ID.
    std::thread::id tid() const
    {
        return _tid;
    }

    /// Return the shared handle pointer context.
    std::shared_ptr<Context<T>> context() const
    {
        return _context;
    }

    /// Assert the call is from the parent event loop thread.
    void assertThread() const
    {
        // assert(uv_thread_equal(&_tid, &uv_thread_self()));
        assert(std::this_thread::get_id() == _tid);
    }

    typedef T Resource;

protected:
    /// Error callback.
    /// Override to handle errors.
    /// The error may be a UV error, or a custom error.
    virtual void onError(const scy::Error& error)
    {
    }

    /// Close callback.
    /// Override to handle closure.
    virtual void onClose()
    {
    }

protected:
    /// NonCopyable and NonMovable
    Handle2(const Handle2&) = delete;
    Handle2& operator=(const Handle2&) = delete;

    uv::Loop* _loop;
    std::shared_ptr<Context<T>> _context;
    std::thread::id _tid = std::this_thread::get_id();
    scy::Error _error;
};


} // namespace uv
} // namespace scy


#endif // SCY_UV_Handle2


/// @\}
