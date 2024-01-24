#include<mymuduo/TcpClient.h>
#include<mymuduo/TcpConnection.h>
#include<mymuduo/Logger.h>
#include<mymuduo/EventLoop.h>
#include<mymuduo/InetAddress.h>
#include<functional>
#include<memory>
#include<string>
class EchoClient
{
public:
  EchoClient(EventLoop* loop, const InetAddress& serverAddr)
      : loop_(loop), client_(loop, serverAddr, "EchoClient")
  {
    client_.setConnectionCallback(
        std::bind(&EchoClient::onConnection, this, std::placeholders::_1));
    client_.setMessageCallback(
        std::bind(&EchoClient::onMessage, this, 
                        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }

  void connect()
  {
    client_.connect();
  }

private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      LOG_INFO ("EchoClient - connected");
      conn->send("hello\n");
    }
    else
    {
      LOG_INFO ("EchoClient - disconnected");
      loop_->quit();
    }
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    std::string msg(buf->retrieveAllAsString());
    LOG_INFO ("EchoClient - received [%d] bytes at[%s]", (int)msg.size(),time.toString().c_str());
    LOG_INFO ("EchoClient -[%s] ", msg.c_str());
  }

  EventLoop* loop_;
  TcpClient client_;
};

int main()
{
  EventLoop loop;
  InetAddress serverAddr( 8888,"127.0.0.1");
  EchoClient client(&loop, serverAddr);

  client.connect();

  loop.loop();
  return 0;
}
