#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <sys/stat.h>
#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include <bits/stdc++.h>
#include <boost/regex.hpp>

using boost::asio::ip::tcp;
//using boost::asio::io_context;
using namespace std;
using namespace boost;

struct req_info{
    int vn;
    int cd;
    string cmd;
    string dst_port;
    string dst_ip;
    string src_ip;
    string src_port;
    string reply;
};

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket, boost::asio::signal_set &signal, boost::asio::io_context& io_context)
    : socket_client(move(socket)),
    socket_server(io_context),
    signal_(signal),
    io_context_(io_context){
      memset(data_, '\0', sizeof(data_));
      memset(reply_, '\0', sizeof(reply_));
  }

  void start(){
    //cout<<"here123123?"<<endl;
    read_req_from_client();
  }

private:
  tcp::socket socket_client;
  tcp::socket socket_server;
  unsigned char data_[1024];
  unsigned char reply_[10];
  boost::asio::signal_set &signal_;
  boost::asio::io_context& io_context_;
  struct req_info req;

  void read_req_from_client(){
    auto self(shared_from_this());
    socket_client.async_read_some(boost::asio::buffer(data_, 1024),
        [this, self](boost::system::error_code ec, std::size_t length){
          if (!ec){
            //cout<<"here?"<<endl;
            parse_request();
            Firewall();
            cout << "<S_IP>: " << req.src_ip << endl;
            cout << "<S_PORT>: " << req.src_port << endl;
            cout << "<D_IP>: " << req.dst_ip << endl;
            cout << "<D_PORT>: " << req.dst_port << endl;
            cout << "<Command>: " << req.cmd << endl;
            cout << "<Reply>: " << req.reply << endl;

            reply_[0] = '\0';
            if(req.reply == "Reject")
              reply_[1] = 91;
            else
              reply_[1] = 90;

            if(req.cd == 1)
              connect_mode();
            
            else if(req.cd == 2)
              bind_mode();
        }
      });
  }

  void read_data_from_client(){
    memset(data_, '\0', sizeof(data_));
    auto self(shared_from_this());
    socket_client.async_read_some(boost::asio::buffer(data_, 1024),
        [this, self](boost::system::error_code ec, size_t length){
            if(!ec){
                write_data_to_server(length);
                //memset(data_, '\0', sizeof(data_));
            }
            else if(ec.value() == 2){   // EOF
                socket_client.close();
                socket_server.close();
                exit(EXIT_SUCCESS);
            }
        });
  }

  void read_data_from_server(){
    memset(data_, '\0', sizeof(data_));
    auto self(shared_from_this());
    socket_server.async_read_some(boost::asio::buffer(data_, 1024),
        [this, self](boost::system::error_code ec, size_t length){
            if(!ec){
                write_data_to_client(length);
                //memset(data_, '\0', sizeof(data_));
              }
            else if(ec.value() == 2){   // EOF
                socket_client.close();
                socket_server.close();
                exit(EXIT_SUCCESS);
            }
        });
  }

  void write_data_to_client(size_t length){
    auto self(shared_from_this());
        boost::asio::async_write(socket_client, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, size_t /*length*/){
                if(!ec){
                    memset(data_, '\0', sizeof(data_));
                    read_data_from_server();
                  }
            });
  }

  void write_data_to_server(size_t length){
    auto self(shared_from_this());
        boost::asio::async_write(socket_server, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, size_t /*length*/){
                if(!ec){
                  memset(data_, '\0', sizeof(data_));
                    read_data_from_client();
                  }
            });
  }

  void write_reply_to_client(){
    auto self(shared_from_this());
        boost::asio::async_write(socket_client, boost::asio::buffer(reply_, 8),
            [this, self](boost::system::error_code ec, size_t /*length*/){
                //cout<<(uint)reply_[1]<<endl;
                if(!ec){
                  memset(reply_, '\0', sizeof(reply_));
                }
            });
  }

  void connect_mode(){
    for(int i=2;i<=7;i++)
      reply_[i] = '\0';

    write_reply_to_client();

    if(req.reply == "Reject")
      exit(EXIT_SUCCESS);

    tcp::resolver r(io_context_);
    tcp::endpoint endpoint_ = r.resolve(req.dst_ip, req.dst_port)->endpoint();

    auto self(shared_from_this());
    socket_server.async_connect(endpoint_, [this, self](boost::system::error_code ec){
      if(!ec){
        read_data_from_client();
        read_data_from_server();
      }
    });
  }

  void bind_mode(){
    for(int i=4;i<=7;i++)
      reply_[i] = '\0';

    if(req.reply == "Reject"){
      reply_[2] = '\0';
      reply_[3] = '\0';
      write_reply_to_client();
      exit(EXIT_SUCCESS);
    }   

    tcp::acceptor acceptor_bind(io_context_, tcp::endpoint(tcp::v4(), 0));
    acceptor_bind.listen();
    unsigned short dst_port = acceptor_bind.local_endpoint().port();
    reply_[2] = (dst_port>>8) & 0X00FF;
    reply_[3] = dst_port & 0X00FF;
    
    write_reply_to_client();
    acceptor_bind.accept(socket_server);
    write_reply_to_client();
    read_data_from_client();
    read_data_from_server();
    return;
  }

  void Firewall(){
    ifstream f("./socks.conf");
    string line = "";
    while(getline(f, line)){
      istringstream ss(line);
      vector<string> tokens;
      string token;
      while(ss >> token)
        tokens.push_back(token);

      if((req.cd==1 && tokens[1]!="c") || (req.cd==2 && tokens[1]!="b"))
        continue;

      string rule = tokens[2];
      replace_all(rule, "*", "[0-9]{1,3}");
      replace_all(rule, ".", "\\.");
      boost::regex expression(rule);
      if(boost::regex_match(req.dst_ip, expression)){
        req.reply = "Accept";
        return;
      }
    }
    req.reply = "Reject";
    return;
  }

  void parse_request(){
      req.vn = data_[0];
      req.cd = data_[1];
      if(req.cd == 1)
        req.cmd = "Connect";
      else if(req.cd == 2)
        req.cmd = "Bind";

      //cout<<req.cd<<endl;
      //cout<<req.cd<<endl;
      //cout<<"test"<<endl;
      req.dst_port = to_string((uint)(data_[2]<<8) | data_[3]);
      if(data_[4]=='\0' && data_[5]=='\0' && data_[6]=='\0' && data_[7]!='\0'){
        //cout<<"test"<<endl;
        int i = 8;
        while(data_[i] != '\0')
          i++;
        i++;
        string domain_name = "";
        while(data_[i] != '\0')
          domain_name += data_[i++];

        //out<<domain_name<<endl;

        tcp::resolver r(io_context_);
        tcp::endpoint endpoint_ = r.resolve(domain_name, req.dst_port)->endpoint();
        req.dst_ip = endpoint_.address().to_string();
      }
      else{
        //cout<<"Enter?"<<endl;
        char dst_ip[18];
        snprintf(dst_ip, 18, "%d.%d.%d.%d", data_[4], data_[5], data_[6], data_[7]);
        req.dst_ip = dst_ip;
        //cout<<req.dst_ip<<endl;
      }

      req.src_ip = socket_client.remote_endpoint().address().to_string();
      req.src_port = to_string(socket_client.remote_endpoint().port());

      return;
  }
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      io_context_(io_context),
      signal_(io_context, SIGCHLD)
  {
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    childHandler();
    do_accept();
  }

private:
  boost::asio::io_context& io_context_;
  boost::asio::signal_set signal_;
  tcp::acceptor acceptor_;

  void childHandler(){
    signal_.async_wait([this](boost::system::error_code ec, int signo){
      if(acceptor_.is_open()){
          int status = 0;
          while (waitpid(-1, &status, WNOHANG) > 0){
              // do nothing
          }
          childHandler();
      }
    });
  }

  void do_accept(){
    //cout<<"here123123 acccc"<<endl;
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket){
          if (!ec){
            //cout<<"accept"<<endl;
            io_context_.notify_fork(boost::asio::io_context::fork_prepare);
            int pid = fork();
            if(pid){
              io_context_.notify_fork(boost::asio::io_context::fork_parent);
              socket.close();
            }
            else{
              //cout<<"child"<<endl;
              io_context_.notify_fork(boost::asio::io_context::fork_child); 
              acceptor_.close();
              signal_.cancel();
              make_shared<session>(move(socket), signal_, io_context_)->start();
            }
          }
          //else{
            //cout<<ec.value()<<endl;
          //}
          do_accept();
        });
  }
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      cerr << "Usage: http_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    server s(io_context, atoi(argv[1]));
    io_context.run();
  }
  catch (std::exception& e)
  {
    cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}