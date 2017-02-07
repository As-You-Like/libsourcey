///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup base
/// @{


#include "scy/stream.h"


using std::endl;


namespace scy {


Stream::Stream(uv::Loop* loop, void* stream)
    : uv::Handle(loop, stream)
    , _buffer(65536)
{
}


Stream::~Stream()
{
    close(); // must close underlying stream
}


void Stream::close()
{
    TraceL << "Close: " << ptr() << std::endl;
    if (active())
        readStop();
    uv::Handle::close();
}


bool Stream::shutdown()
{
    assertThread();

    TraceL << "Send shutdown" << std::endl;
    if (!active()) {
        WarnL << "Cannot shutdown a closed stream" << std::endl;
        return false;
    }

    // XXX: Sending shutdown causes an eof error to be
    // returned via handleRead() which sets the stream
    // to error state. This is not really an error,
    // perhaps it should be handled differently?
    int r = uv_shutdown(new uv_shutdown_t, ptr<uv_stream_t>(), [](uv_shutdown_t* req, int) {
        delete req;
    });

    return r == 0;
}


bool Stream::write(const char* data, std::size_t len)
{
    assertThread();

    // if (closed())
    //    throw std::runtime_error("IO error: Cannot write to closed stream");
    if (!active())
        return false;

    int r;
    uv_write_t* req = new uv_write_t;
    uv_buf_t buf = uv_buf_init((char*)data, (int)len);
    uv_stream_t* stream = this->ptr<uv_stream_t>();
    bool isIPC = stream->type == UV_NAMED_PIPE &&
                 reinterpret_cast<uv_pipe_t*>(stream)->ipc;

    if (!isIPC) {
        r = uv_write(req, stream, &buf, 1, [](uv_write_t* req, int) {
            delete req;
        });
    } else {
        r = uv_write2(req, stream, &buf, 1, nullptr, [](uv_write_t* req, int) {
            delete req;
        });
    }

    if (r) {
        delete req;
        assert(0);
        // setAndThrowError(r, "Stream write error");
    }
    return r == 0;
}


Buffer& Stream::buffer()
{
    assertThread();
    return _buffer;
}


bool Stream::closed() const
{
    return uv::Handle::closed();
}


bool Stream::readStart()
{
    // TraceL << "Read start: " << ptr() << std::endl;
    int r = uv_read_start(this->ptr<uv_stream_t>(), Stream::allocReadBuffer, handleRead);
    if (r)
        setUVError("Stream read error", r);
    return r == 0;
}


bool Stream::readStop()
{
    //std::cout << _ptr << " stream read stop" << std::endl;
    // TraceL << "Read stop: " << ptr() << std::endl;
    int r = uv_read_stop(ptr<uv_stream_t>());
    if (r)
        setUVError("Stream read error", r);
    return r == 0;
}

void Stream::onRead(const char* data, std::size_t len)
{
    // TraceL << "On read: " << len << std::endl;
    Read.emit(*this, data, (const int)len);
}

void Stream::handleReadCommon(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf, uv_handle_type pending)
{
    // TraceL << "Handle read: " << nread << std::endl;
    auto self = reinterpret_cast<Stream*>(handle->data);

    if (nread >= 0) {
        self->onRead(buf->base, nread);
    } else {
        // The stream was closed in error
        // The value of nread is the error number
        // ie. UV_ECONNRESET or UV_EOF etc ...
        self->setUVError("Stream error", (int)nread);
    }
}


//
/// UV callbacks
//


void Stream::handleRead(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
{
    handleReadCommon(handle, nread, buf, UV_UNKNOWN_HANDLE);
}

void Stream::handleRead2(uv_pipe_t* handle, ssize_t nread, const uv_buf_t* buf, uv_handle_type pending)
{
    handleReadCommon((uv_stream_t*)handle, nread, buf, pending);
}

void Stream::allocReadBuffer(uv_handle_t* handle, std::size_t suggested_size, uv_buf_t* buf)
{
    auto self = reinterpret_cast<Stream*>( handle->data);

    // Reserve the recommended buffer size
    // if (suggested_size > self->_buffer.capacity())
    // self->_buffer.capacity(suggested_size);
    assert(self->_buffer.size() >= suggested_size);

    // Reset the buffer position on each read
    buf->base = self->_buffer.data();
    buf->len = self->_buffer.size();
}


} // namespace scy


/// @\}
