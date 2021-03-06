#include <pichi/net/asio.hpp>
#include <pichi/net/common.hpp>
#include <pichi/net/direct.hpp>
#include <utility>

using namespace std;
namespace asio = boost::asio;

namespace pichi::net {

DirectAdapter::DirectAdapter(Socket&& socket) : socket_{move(socket)} {}

size_t DirectAdapter::recv(MutableBuffer<uint8_t> buf, Yield yield)
{
  return socket_.async_read_some(asio::buffer(buf), yield);
}

void DirectAdapter::send(ConstBuffer<uint8_t> buf, Yield yield) { write(socket_, buf, yield); }

void DirectAdapter::close() { socket_.close(); }

bool DirectAdapter::readable() const { return socket_.is_open(); }

bool DirectAdapter::writable() const { return socket_.is_open(); }

void DirectAdapter::connect(Endpoint const& remote, Endpoint const& server, Yield yield)
{
  pichi::net::connect(server, socket_, yield);
}

} // namespace pichi::net
