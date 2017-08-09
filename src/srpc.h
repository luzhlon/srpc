#ifndef __SRPC_H__
#define __SRPC_H__

#include <mutex>
#include "tstream.hpp"

#include "xval.h"
#include "xval_val.h"
#include "xval_list.h"
#include "xval_dict.h"

namespace srpc {
    using namespace xval;

    class Session;
    class Server;
    class Client;

    enum msg_t {
        MSG_INVALID = 0,
        MSG_CALL,
        MSG_NOTIFY,
        MSG_RETURN,
        MSG_EXCEPT,
        MSG_CLOSE
    };

    enum err_code {
        ERR_NO_FUNC
    };

    typedef void (*Function)(Session& h, Tuple& args);
    typedef void (*OnOpen)(Session& h);
    typedef void (*OnClose)(Session& h, bool except);

    class EXPORT Session {
    public:
        Session() {}
        Session(tstream&& ts) : _ts(std::move(ts)) {}

        Session& operator=(tstream&& ts) {
            _ts = std::move(ts);
            return *this;
        }

        void setAttr(const Value& k, const Value& v) {
            _attr.dict().set(k, v);
        }
        Value getAttr(const Value& k) {
            return _attr.dict().get(k);
        }
        // send a pack 'call' or 'notify'
        bool invoke(const Value& func, const Value *argv, size_t argc, bool notify = false);
        // send a pack 'call'
        Value call(const Value& func, const Value *data, size_t count);
        Value call(const Value& func, const Value& args) {
            return call(func, &args, 1);
        }
        Value call(const Value& func) {
            return call(func, nullptr, 0);
        }
        Value call(const Value& func, initializer_list<Value> args) {
            return call(func, args.begin(), args.size());
        }
        // send a pack 'return'
        void retn(const Value *data, size_t count);
        void retn(initializer_list<Value> args) {
            return retn(args.begin(), args.size());
        }
        void retn(const Value& args) {
            return retn(&args, 1);
        }
        // send a pack 'notify'
        bool notify(const Value& func, const Value& args) {
            return invoke(func, &args, 1, true);
        }
        // send a pack 'except'
        void except(err_code err);

        void addfunc(const Value& fid, Function f) {
            _funs.set(fid, (void*)f);
        }

        Function getfunc(const Value& id) {
            auto f = _funs.get(id);
            return f.ispointer() ? (Function)f.pointer() : nullptr;
        }

        void run();

        OnOpen onopen = nullptr;
        OnClose onclose = nullptr;

    private:
        void handle_invoke();
        Value wait_return();
        // about the pack
        bool send_pack();
        bool recv_pack();

        inline msg_t type() { return _type; }
        inline bool typeis(msg_t t) {
            return type() == t;
        }

        recursive_mutex _mutex;
        Value _funs = Dict::New(8);
        Value _pack = List::New(8);     // pack's buffer of list
        Value _attr = Dict::New(0);     // session's attributes
        msg_t _type;
        bool _returned = false;
        bool _isnotify;
        bool _closed = false;

    protected:
        friend class srpc::Server;
        friend class srpc::Client;
        tstream _ts;
    };

    class EXPORT Client : public Session {
    public:
        bool connect(const char *addr, unsigned short port) {
            return _ts.connect(addr, port);
        }
    };

    class EXPORT Server : public tstream::server {
    public:
        template<typename... Args>
        Server(Args... args) : tstream::server(args...) {}

        Session accept() { return server::accept(); }
    };
}

#endif /* __SRPC_H__ */
