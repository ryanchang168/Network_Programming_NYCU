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

using boost::asio::ip::tcp;
using boost::asio::io_context;
using namespace std;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket, boost::asio::signal_set &signal, boost::asio::io_context& io_context)
    : socket_(move(socket)),
    signal_(signal),
    io_context_(io_context){

  }

  void start(){
    do_read();
  }

private:
  tcp::socket socket_;
  char data_[1024];
  map <string, string> env_var;
  boost::asio::signal_set &signal_;
  boost::asio::io_context& io_context_;

  void do_read(){
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, 1024),
        [this, self](boost::system::error_code ec, std::size_t length){
          if (!ec){
            parse_header();
            fork_child();
          }
        });
  }

  void parse_header(){
    string data = data_;
    memset(data_, '\0', sizeof(data_));
    boost::replace_all(data, "\r", "");

    vector<string> v;
    boost::split(v, data, boost::is_any_of("\n"));

    vector<string> args;
    boost::split(args, v[0], boost::is_any_of(" "), boost::token_compress_on);
    env_var["REQUEST_METHOD"] = args[0];
    env_var["REQUEST_URI"] = args[1];
    env_var["SERVER_PROTOCOL"] = args[2];
    if(args[1].find("?") != string::npos)
        env_var["QUERY_STRING"] = args[1].substr(args[1].find("?")+1);
    else 
        env_var["QUERY_STRING"] = "";

    args.clear();
    boost::split(args, v[1], boost::is_any_of(" "), boost::token_compress_on);
    env_var["HTTP_HOST"] = args[1];

    env_var["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
    env_var["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
    env_var["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
    env_var["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());
  }

  void fork_child(){
    io_context_.notify_fork(boost::asio::io_context::fork_prepare);
    int pid = fork();

    if(pid == 0){
        io_context_.notify_fork(boost::asio::io_context::fork_child);
        signal_.cancel();
        set_env();
        exec_res();
    }
    else{
        io_context_.notify_fork(boost::asio::io_context::fork_parent);
        socket_.close();
    }
  }

  void set_env(){
    for(map<string, string>::iterator it = env_var.begin(); it != env_var.end(); it++)
        setenv((it->first).c_str(), (it->second).c_str(), 1);    
  }

  void dup_fd(){
    dup2(socket_.native_handle(), 0);
    dup2(socket_.native_handle(), 1);
    dup2(socket_.native_handle(), 2);
  }

  void exec_res(){
    string s = env_var["REQUEST_URI"];
    string file_name = "." + (s.find("?") != string::npos ? s.substr(0, s.find("?")) : s);
    string reply = "";

    struct stat buf;
    if(stat(file_name.c_str(), &buf) != -1 && S_ISREG(buf.st_mode))   // is regular file?
        reply += "HTTP/1.1 200 OK\r\n";
    else
        reply += "HTTP/1.1 404 NOT FOUND\r\n\r\n";

    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(reply, reply.length()),
          [this, self](boost::system::error_code ec, size_t len){
              if(!ec){
                // clients will start handling 
              }
          });

    if(stat(file_name.c_str(), &buf) != -1 && S_ISREG(buf.st_mode)){
        dup_fd();
        socket_.close();

        char* c_file[] = {strdup(file_name.c_str()), NULL};
        execvp(file_name.c_str(), c_file);
    }
    else{
        socket_.close();
        exit(EXIT_SUCCESS);
    }
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
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket){
          if (!ec)
            std::make_shared<session>(move(socket), signal_, io_context_)->start();
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
      std::cerr << "Usage: http_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    server s(io_context, atoi(argv[1]));
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}