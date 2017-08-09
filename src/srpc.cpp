
#include <thread>
#include <assert.h>

#include "xval_mp.h"
#include "srpc.h"

using namespace xval;

namespace srpc {

    void Session::handle_invoke() {
        // check the type
        auto t = type();
        _isnotify = MSG_NOTIFY == t;
        assert(MSG_CALL == t || _isnotify);
        // get the function to invoke
        auto& pack = _pack.list();
        auto funid = pack[1];       // function id
        // call the function
        auto func = getfunc(funid);
        if (func) {
            Value args, nil;
            args = Tuple::New(pack.begin() + 2, pack.size() - 2);
            func(*this, args.tuple());
            // ensure return
            retn(nil);
            // reset, for next return
            _returned = false;
        } else if (t == MSG_CALL) {
            return except(ERR_NO_FUNC);
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

    void Session::retn(const Value *data, size_t count) {
        if (_returned || _isnotify)
            return;
        _mutex.lock();
        auto& pack = _pack.list();
        pack.resize(1);
        pack.set(0, (int64_t)MSG_RETURN);
        while (count--)
            pack.append(*data++);
        send_pack();
        _mutex.unlock();
        _returned = true;
    }

    void Session::except(err_code err) {
        auto& pack = _pack.list();
        _mutex.lock();
        pack.resize(2);
        pack.set(0, (int64_t)MSG_EXCEPT);
        pack.set(1, (int64_t)err);
        send_pack();
        _mutex.lock();
    }

    Value Session::wait_return() {
        do {
            if (!recv_pack()) break;
            if (typeis(MSG_RETURN)) {
                auto& pack = _pack.list();
                auto data = pack[1];
                if (pack.size() > 2)
                    data = Tuple::New(pack.begin() + 1, pack.size() - 1);
                return data;
            } else {
                handle_invoke();
            }
        } while (true);
        return Value();
    }

    bool Session::send_pack() {
        if (_closed) return false;
        return mp_pack(_pack, _ts);
    }

    bool Session::recv_pack() {
        auto s = mp_unpack(_pack, _ts);
        if (!s) return false;
        _type = (msg_t)_pack.list()[0].Int(0);
        if (type() == MSG_CLOSE)
            _closed = true;
        return true;
    }

    void Session::run() {
        if (onopen) onopen(*this);
        while (recv_pack())
            handle_invoke();
        if (onclose) onclose(*this, !_closed);
    }
}
