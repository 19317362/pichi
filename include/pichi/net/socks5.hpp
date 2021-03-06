#ifndef PICHI_NET_SOCKS5_HPP
#define PICHI_NET_SOCKS5_HPP

#include <boost/asio/ip/tcp.hpp>
#include <pichi/net/adapter.hpp>

namespace pichi::net {

class Socks5Adapter : public Ingress, public Egress {
private:
  using Socket = boost::asio::ip::tcp::socket;

public:
  Socks5Adapter(Socket&&);
  ~Socks5Adapter() override = default;

public:
  size_t recv(MutableBuffer<uint8_t>, Yield) override;
  void send(ConstBuffer<uint8_t>, Yield) override;
  void close() override;
  bool readable() const override;
  bool writable() const override;
  Endpoint readRemote(Yield) override;
  void connect(Endpoint const& remote, Endpoint const& next, Yield) override;
  void confirm(Yield) override;

private:
  Socket socket_;
};

} // namespace pichi::net

#endif // PICHI_NET_SOCKS5_HPP
