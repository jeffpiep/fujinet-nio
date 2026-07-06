#pragma once

#include "fujinet/io/core/io_message.h"

#include <chrono>

namespace fujinet::io {

// Forward declaration
class IORequest;
class IOResponse;

// Abstract transport: turns bytes on a Channel into IORequests/IOResponses.
class ITransport {
public:
    virtual ~ITransport() = default;

    // Called every loop iteration so the transport can do background work
    // (e.g. timeouts, internal state machines). Default: no-op.
    virtual void poll() {}

    // Optionally wait until this transport may have work for the next core
    // tick. This lets platform loops keep a normal idle heartbeat while
    // latency-sensitive channels wake the loop early.
    virtual bool supports_work_wait() const { return false; }

    virtual bool wait_for_work(std::chrono::milliseconds timeout) {
        (void)timeout;
        return false;
    }

    // Try to read and parse one complete request from this transport.
    // Returns true if a full request was produced and stored in outReq.
    // Returns false if no complete request is available right now.
    virtual bool receive(IORequest& outReq) = 0;

    // Send a response back over this transport.
    virtual void send(const IOResponse& resp) = 0;
};

} // namespace fujinet::io
