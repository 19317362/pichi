#include <iostream>
#include <pichi/api/egress_manager.hpp>
#include <pichi/api/ingress_manager.hpp>
#include <pichi/api/router.hpp>
#include <pichi/api/session.hpp>
#include <pichi/asserts.hpp>
#include <pichi/net/adapter.hpp>
#include <pichi/net/asio.hpp>
#include <pichi/net/helpers.hpp>

using namespace std;
namespace asio = boost::asio;
namespace ip = asio::ip;
namespace sys = boost::system;
using ip::tcp;

namespace pichi::api {

IngressManager::IngressManager(Strand strand, Router const& router, EgressManager const& eManager)
  : strand_{strand}, router_{router}, eManager_{eManager}, c_{}
{
}

IngressManager::ValueType IngressManager::generatePair(DelegateIterator it)
{
  return make_pair(cref(it->first), cref(it->second.first));
}

void IngressManager::logging(exception_ptr eptr)
{
  try {
    if (eptr) rethrow_exception(eptr);
  }
  catch (Exception& e) {
    cout << "Pichi Error: " << e.what() << endl;
  }
  catch (sys::system_error& e) {
    if (e.code() == asio::error::eof || e.code() == asio::error::operation_aborted) return;
    cout << "Socket Error: " << e.what() << endl;
  }
}

void IngressManager::stub(exception_ptr) {}

template <typename Function, typename FaultHandler>
void IngressManager::spawn(Function&& func, FaultHandler&& faultHandler)
{
  asio::spawn(strand_, [f = forward<Function>(func),
                        fh = forward<FaultHandler>(faultHandler)](auto yield) mutable {
    try {
      f(yield);
    }
    catch (...) {
      fh(current_exception());
      logging(current_exception());
    }
  });
}

IngressManager::ConstIterator IngressManager::begin() const noexcept
{
  return {cbegin(c_), cend(c_), &IngressManager::generatePair};
}

IngressManager::ConstIterator IngressManager::end() const noexcept
{
  return {cend(c_), cend(c_), &IngressManager::generatePair};
}

void IngressManager::listen(typename Container::iterator it, asio::yield_context yield)
{
  auto& acceptor = it->second.second;

  while (true) {
    spawn([s = acceptor.async_accept(yield), &vo = as_const(it->second.first),
           &iname = as_const(it->first), this](auto yield) mutable {
      auto ingress = net::makeIngress(vo, move(s));
      auto remote = ingress->readRemote(yield);
      auto resolve = [&yield, &remote, &io = strand_.context()]() {
        auto ec = sys::error_code{};
        auto r = tcp::resolver{io}.async_resolve(remote.host_, remote.port_, yield[ec]);
        return ec ? tcp::resolver::results_type{} : r;
      };
      auto it = eManager_.find(router_.route(remote, iname, vo.type_, resolve));
      assertFalse(it == cend(eManager_), PichiError::MISC);
      auto session =
          make_shared<Session>(strand_.context(), move(ingress),
                               net::makeEgress(it->second, tcp::socket{strand_.context()}));
      session->start(remote, it->second.type_ == AdapterType::DIRECT ||
                                     it->second.type_ == AdapterType::REJECT ?
                                 remote :
                                 net::Endpoint{net::detectHostType(*it->second.host_),
                                               *it->second.host_, to_string(*it->second.port_)});
    });
  }
}

void IngressManager::update(string const& name, IngressVO ivo)
{
  assertFalse(ivo.type_ == AdapterType::DIRECT, PichiError::MISC);
  assertFalse(ivo.type_ == AdapterType::REJECT, PichiError::MISC);
  auto it = c_.find(name);
  if (it == std::end(c_)) {
    auto p = c_.try_emplace(
        name, make_pair(move(ivo),
                        Acceptor{strand_.context(), {ip::make_address(ivo.bind_), ivo.port_}}));
    assertTrue(p.second, PichiError::MISC);
    it = p.first;
  }
  else {
    it->second =
        make_pair(move(ivo), Acceptor{strand_.context(), {ip::make_address(ivo.bind_), ivo.port_}});
  }
  spawn([it, this](auto yield) { listen(it, yield); },
        [it, this](auto eptr) {
          try {
            if (eptr) rethrow_exception(eptr);
          }
          catch (sys::system_error const& e) {
            if (e.code() != asio::error::operation_aborted) {
              assert(it != std::end(c_));
              c_.erase(it);
            }
          }
        });
}

void IngressManager::erase(string_view name)
{
  auto it = c_.find(name);
  if (it != std::end(c_)) c_.erase(it);
}

} // namespace pichi::api
