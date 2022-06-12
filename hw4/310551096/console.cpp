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
//using boost::asio::io_context;
using namespace std;
using namespace boost;
tcp::resolver::results_type::iterator it;

string socks_ip="", socks_port="";
vector<string> info;

class session
  : public std::enable_shared_from_this<session> 
{
public:
  session(boost::asio::io_context& io_context, string file_dir, int server_id)
        : io_context_(io_context),
          socket_(io_context),
          file_dir_("./test_case/" + file_dir),
          server_id_(server_id),
          resolver_(io_context)
    {
            memset(data_, '\0', sizeof(data_));
            memset(req_reply, '\0', sizeof(req_reply));
    }

  void start(){
    connect_server();
  }

private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    ifstream file_dir_;
    int server_id_;
    tcp::resolver resolver_;
    boost::asio::ip::tcp::endpoint endpoint2;
    char data_[1024];
    unsigned char req_reply[10];
    string line_;

    void connect_server(){
        auto self(shared_from_this());
        tcp::resolver::query q(info[server_id_*3], info[server_id_*3+1]);
        resolver_.async_resolve(q, [this, self](boost::system::error_code ec, tcp::resolver::iterator resolve_it){
            if(!ec){
                endpoint2 = *resolve_it;
                socket_.async_connect(it->endpoint(), [this, self](boost::system::error_code ec){
                if(!ec)
                    write_req_to_socks();
                });
            }
        });
    }

    void do_read(){
        //cerr<<"OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO"<<flush;
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, 1024),
            [this, self](boost::system::error_code ec, size_t len){
                for(int i=0;i<10;i++)
                    cout<< "<script>console.log(\"" << (uint)data_[i]<< ")\"</script>\n";
                if(!ec){
                    string data = data_;
                    //cout<<"22OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO"<<data.length()<<flush;
                    memset(data_, '\0', sizeof(data_));
                    string tmp_data = replace_char(data);
                    cout << "<script>document.getElementById('s" << server_id_ << "').innerHTML += '" << tmp_data << "';</script>" << flush;
                    if(data.find("% ") != string::npos)
                        do_write();
                    else
                        do_read();
                }
                //else
                    //cout<<"?????"<<ec.message()<<flush;
            });
    }

    void do_write(){
        auto self(shared_from_this());
        getline(file_dir_, line_);
        line_ += "\n";
        string line = replace_char(line_);
        //cout<< "<script>console.log(\"" << line << "\"</script>\n";
        cout << "<script>document.getElementById('s" << server_id_ << "').innerHTML += '<b>" << line << "</b>';</script>" << flush;

       
        boost::asio::async_write(socket_, boost::asio::buffer(line_, line_.length()),
            [this, self](boost::system::error_code ec, size_t len){
                if(!ec){
                    //cout<<"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"<<flush;
                    line_.clear();
                    do_read();
                }
                //else
                    //cout<<"ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"<<flush;
            });
    }

    void write_req_to_socks(){
        memset(req_reply, '\0', sizeof(req_reply));
        string ip_str = endpoint2.address().to_string();
        short port_num = endpoint2.port();
        vector<string> ip;
        split(ip, ip_str, is_any_of("."), token_compress_on);

        req_reply[0] = 4;
        req_reply[1] = 1;
        req_reply[2] = port_num / 256;
        req_reply[3] = port_num % 256;
        for(int i=4;i<=7;i++)
            req_reply[i] = stoi(ip[i-4]);
        //cout<<req_reply[2]<<endl;
        //cout<<req_reply[3]<<endl;
        //cout<<req_reply[4]<<endl;

        write_req();
        return;
    }

    void write_req(){
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(req_reply, 9),
            [this, self](const boost::system::error_code& ec, std::size_t) {
                if(!ec){
                    read_reply_from_socks();
                }
            }
        );
    }

    void read_reply_from_socks(){
        auto self(shared_from_this());
        memset(req_reply, '\0', sizeof(req_reply));
        socket_.async_read_some(boost::asio::buffer(req_reply, 1024), 
            [this, self](boost::system::error_code ec, std::size_t length) {
                for(int i=0;i<10;i++)
                    cout<< "<script>console.log(\"" << (uint)req_reply[i] << length << ")\"</script>\n";
                if(!ec){
                    if((uint)req_reply[1] == 90)
                        do_read();
                }
            }
        );
    }

    string replace_char(string data){
        boost::replace_all(data, "\r", "");
        boost::replace_all(data, "\n", "&NewLine;");
        boost::replace_all(data, "\'", "&apos;");
        boost::replace_all(data, "\"", "&quot;");
        boost::replace_all(data, "<", "&lt;");
        boost::replace_all(data, ">", "&gt;");

        return data;
    }
};

vector<string> parse_query(string q){
    vector<string> args;
    boost::split(args, q, boost::is_any_of("&"), boost::token_compress_on);

    vector<string> info;
    for(int i=0;i<args.size()-2;i+=3){
        if(args[i][args[i].size()-1] == '=')
            break;
        info.push_back(args[i].substr(args[i].find("=")+1));
        info.push_back(args[i+1].substr(args[i+1].find("=")+1));
        info.push_back(args[i+2].substr(args[i+2].find("=")+1));
    }

    socks_ip = args[args.size()-2].substr(args[args.size()-2].find("=")+1);
    socks_port = args[args.size()-1].substr(args[args.size()-1].find("=")+1);

    //cout<<socks_ip<<endl;
    //cout<<socks_port<<endl;

    return info;
}

string Content(vector<string> info){
    string http_content = "";
    http_content = 
        http_content \
        + "<!DOCTYPE html>\n"
        + "<html lang=\"en\">\n"
        + "  <head>\n"
        + "    <meta charset=\"UTF-8\" />\n"
        + "    <title>NP Project 3 Sample Console</title>\n"
        + "    <link\n"
        + "      rel=\"stylesheet\"\n"
        + "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
        + "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
        + "      crossorigin=\"anonymous\"\n"
        + "    />\n"
        + "    <link\n"
        + "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        + "      rel=\"stylesheet\"\n"
        + "    />\n"
        + "    <link\n"
        + "      rel=\"icon\"\n"
        + "      type=\"image/png\"\n"
        + "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
        + "    />\n"
        + "    <style>\n"
        + "      * {\n"
        + "        font-family: 'Source Code Pro', monospace;\n"
        + "        font-size: 1rem !important;\n"
        + "      }\n"
        + "      body {\n"
        + "        background-color: #212529;\n"
        + "      }\n"
        + "      pre {\n"
        + "        color: #cccccc;\n"
        + "      }\n"
        + "      b {\n"
        + "        color: #01b468;\n"
        + "      }\n"
        + "    </style>\n"
        + "  </head>\n";
    http_content = 
        http_content \
        + "<body>\n"
        + "    <table class=\"table table-dark table-bordered\">\n"
        + "      <thead>\n"
        + "        <tr>\n";
    
    for(int i=0;i<info.size();i+=3) {
        string server_info = info[i] + ":" + info[i+1];
        http_content += "          <th scope=\"col\">" + server_info + "</th>\n";
    }
    
    http_content = 
        http_content \
        + "        </tr>\n"
        + "      </thead>\n"
        + "      <tbody>\n"
        + "        <tr>\n";

    for(int i=0;i<info.size();i+=3) {    
        string server_id = "s" + to_string(i/3);
        http_content += "          <td><pre id=\"" + server_id + "\" class=\"mb-0\"></pre></td>\n";
    }

    http_content =
        http_content \
        + "        </tr>\n"
        + "      </tbody>\n"
        + "    </table>\n"
        + "  </body>\n"
        + "</html>\n";

    return http_content;
 }


int main(){
    try
  {
    info = parse_query(string(getenv("QUERY_STRING")));
    string http_header = "Content-type: text/html\r\n\r\n";
    string http_content = Content(info);

    cout << http_header << flush;
    cout << http_content << flush;

    boost::asio::io_context io_context;
    tcp::resolver r(io_context);
    tcp::resolver::query q(socks_ip, socks_port);
    tcp::resolver::results_type endpoint1 = r.resolve(socks_ip, socks_port);
    it = endpoint1.begin();

    for(int i=0;i<info.size();i+=3)
       make_shared<session>(io_context, info[i+2], int(i/3))->start();
    
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}