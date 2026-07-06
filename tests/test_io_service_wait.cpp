#include "doctest.h"

#include "fujinet/core/core.h"
#include "fujinet/io/core/io_message.h"
#include "fujinet/io/core/request_handler.h"
#include "fujinet/io/transport/io_service.h"
#include "fujinet/io/transport/transport.h"

#include <chrono>
#include <utility>
#include <vector>

namespace {

class FakeRequestHandler final : public fujinet::io::IRequestHandler {
public:
    fujinet::io::IOResponse handleRequest(const fujinet::io::IORequest& request) override
    {
        fujinet::io::IOResponse response{};
        response.id = request.id;
        response.deviceId = request.deviceId;
        response.command = request.command;
        response.status = fujinet::io::StatusCode::Ok;
        return response;
    }
};

class WaitableTransport final : public fujinet::io::ITransport {
public:
    explicit WaitableTransport(bool supportsWait, std::vector<bool> waitResults = {})
        : _supportsWait(supportsWait)
        , _waitResults(std::move(waitResults))
    {
    }

    bool supports_work_wait() const override
    {
        return _supportsWait;
    }

    bool wait_for_work(std::chrono::milliseconds timeout) override
    {
        _waitCalls.push_back(timeout);
        if (_waitResults.empty()) {
            return false;
        }
        const bool result = _waitResults.front();
        _waitResults.erase(_waitResults.begin());
        return result;
    }

    bool receive(fujinet::io::IORequest&) override
    {
        return false;
    }

    void send(const fujinet::io::IOResponse&) override
    {
    }

    const std::vector<std::chrono::milliseconds>& wait_calls() const
    {
        return _waitCalls;
    }

private:
    bool _supportsWait;
    std::vector<bool> _waitResults;
    std::vector<std::chrono::milliseconds> _waitCalls;
};

} // namespace

TEST_CASE("IOService reports whether any transport supports waiting")
{
    FakeRequestHandler handler;
    fujinet::io::IOService service(handler);

    WaitableTransport unsupported(false);
    service.addTransport(&unsupported);

    CHECK_FALSE(service.hasWaitableWorkSource());
    CHECK_FALSE(service.waitForWork(std::chrono::milliseconds(50)));
    CHECK(unsupported.wait_calls().empty());

    WaitableTransport supported(true);
    service.addTransport(&supported);

    CHECK(service.hasWaitableWorkSource());
}

TEST_CASE("IOService checks immediate transport work before bounded wait")
{
    FakeRequestHandler handler;
    fujinet::io::IOService service(handler);
    WaitableTransport transport(true, {false, true});
    service.addTransport(&transport);

    CHECK(service.waitForWork(std::chrono::milliseconds(50)));

    REQUIRE(transport.wait_calls().size() == 2);
    CHECK(transport.wait_calls()[0] == std::chrono::milliseconds(0));
    CHECK(transport.wait_calls()[1] == std::chrono::milliseconds(50));
}

TEST_CASE("FujinetCore exposes transport waitability to application loops")
{
    fujinet::core::FujinetCore core;
    WaitableTransport transport(true, {true});

    CHECK_FALSE(core.hasWaitableWorkSource());
    core.addTransport(&transport);

    CHECK(core.hasWaitableWorkSource());
    CHECK(core.waitForWork(std::chrono::milliseconds(25)));
}
