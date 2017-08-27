
#include <thread>
#include <assert.h>

#include "xval_mp.h"
#include "srpc.h"

using namespace xval;

namespace srpc {

    Session& Session::operator=(tstream&& ts) {
        _ts = std::move(ts);
        _closed = false;
        return *this;
    }

    inline msg_t basic(msg_t t) { return (msg_t)((uint32_t)t & 0xF0); }

    void Session::handle_invoke() {
        // check the type
        auto t = type();
        assert(basic(t) == MSG_INVOKE);
        if (t == MSG_CLOSE)
            _closed = true;
        else {
            // get the function to invoke
            auto& pack = _pack.list();
            auto fid = pack[1];       // function id
            // call the function
            auto func = getfunc(fid);
            if (func) {
                Value args, nil;
                args = Tuple::New(pack.begin() + 2, pack.size() - 2);
                // store the original value of thus tow variable
                auto isnotify = _isnotify, returned = _returned;
                _isnotify = MSG_NOTIFY == t, _returned = false;
                func(*this, args.tuple());  // call the rpc function
                retn(nil);                  // ensure return
                // restore the original value of thus tow variable
                _isnotify = isnotify, _returned = returned;
            } else if (t == MSG_CALL)
                retn(nullptr, 0, MSG_NOFUNC);
        }
    }

    bool Session::invoke(const Value& fid, const Value *argv, size_t argc, bool notify) {
        _mutex.lock();
        // construct the pack
        auto& pack = _pack.list();
        pack.resize(2);
        pack.set(0, (int64_t)(notify ? MSG_NOTIFY : MSG_CALL));
        pack.set(1, fid);
        while (argc--)
            pack.append(*argv++);
        auto b = send_pack();
        _mutex.unlock();
        return b;
    }

    Value Session::call(const Value& func, const Value *data, size_t count) {
        if (invoke(func, data, count))
            return wait_return();
        // error handle
        // ...
        return Value();
    }

    void Session::retn(const Value *data, size_t count, msg_t t) {
        if (_returned || _isnotify)
            return;
        _mutex.lock();
        auto& pack = _pack.list();
        pack.resize(1);
        pack.set(0, (uint64_t)t);
        while (count--)
            pack.append(*data++);
        send_pack();
        _mutex.unlock();
        _returned = true;
    }

    void Session::close() {
        auto& pack = _pack.list();
        _mutex.lock();
        pack.resize(1);
        pack.set(0, (int64_t)MSG_CLOSE);
        send_pack();
        _mutex.unlock();
        _closed = true;
        _ts.close();
    }

    Value Session::wait_return() {
        Value data;
        while (recv_pack()) {
            auto t = type();
            if (basic(t) == MSG_INVOKE)
                handle_invoke();
            else if (t == MSG_RETVAL) {
                auto& pack = _pack.list();
                data = pack.size() > 2 ?
                    Tuple::New(pack.begin() + 1, pack.size() - 1): pack[1];
                break;
            } else if (t == MSG_NOFUNC) {
                // error handle
                break;
            }
        }
        return data;
    }

    bool Session::send_pack() {
        if (isclosed()) return false;
        return mp_pack(_pack, _ts);
    }

    bool Session::recv_pack() {
        if (isclosed()) return false;
        return mp_unpack(_pack, _ts);
    }

    void Session::run() {
        if (onopen) onopen(*this);
        while (recv_pack() && isopened())
            handle_invoke();
        if (onclose) onclose(*this, !_closed);
    }
}
