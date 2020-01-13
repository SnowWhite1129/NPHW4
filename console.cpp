#include <utility>
#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <regex>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <boost/algorithm/string/replace.hpp>
#include <fstream>
#include <err.h>


using namespace std;
using namespace boost::asio;
using namespace boost::system;
using namespace ip;

io_service global_io_service;

void output_shell(int ID, string content){
    boost::replace_all(content, "\r\n", "&NewLine;");
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "\\", "\\\\");
    boost::replace_all(content, "\'", "\\\'");
    boost::replace_all(content, "<", "&lt;");
    boost::replace_all(content, ">", "&gt;");
    string session = "s" + to_string(ID);
    cout << "<script>document.getElementById('" << session << "').innerHTML += '" << content << "';</script>" << endl; 
    fflush(stdout);
}

void output_command(int ID, string content){
    boost::replace_all(content,"\r\n","&NewLine;");
    boost::replace_all(content,"\n","&NewLine;");
    boost::replace_all(content, "\\", "\\\\");
    boost::replace_all(content,"\'","\\\'");
    boost::replace_all(content,"<","&lt;");
    boost::replace_all(content,">","&gt;");
    string session = "s" + to_string(ID);
    cout << "<script>document.getElementById('" << session << "').innerHTML += '<b>" << content << "</b>';</script>" << endl;
    fflush(stdout);
}


class ShellSession :public enable_shared_from_this<ShellSession>{
private:
    enum { max_length = 1024 };
    tcp::socket _socket;
    tcp::resolver _resolver;
    tcp::resolver _sock_resolver;
    tcp::resolver::query _query;
    tcp::resolver::query _sock_query;

    string _dstIP;
    string _dstport;
    string _hostname;
    string _port;
    string _filename;
    array<char, max_length> _data;
    ifstream _in;
    int _session;
    vector<unsigned char> in_buf_;
public:
    ShellSession(string hostname, string port, string filename, int session, string sockhost, string sockport):
    _socket(global_io_service),
    _resolver(global_io_service),
    _sock_resolver(global_io_service),
    _query(tcp::v4(), hostname, port),
    _sock_query(tcp::v4(), sockhost, sockport),
    _hostname(hostname),
    _port(port),
    _filename(filename),
    _in("test_case/" + _filename),
    _session(session),
    in_buf_(8){}
    void start(){
        do_resolve();
    }
private:
    void send_socks4_request(){
        auto self(shared_from_this());
	
//	    cout << "Sending request ..." << endl;

        in_buf_[0] = 0x04;
        in_buf_[1] = 0x01;

        int port = stoi(_dstport);
        in_buf_[2] = port / 256;
        in_buf_[3] = port % 256;

        for (int i = 0; i < 4; ++i) {
            size_t pos = _dstIP.find('.');
            in_buf_[i+4] = stoi(_dstIP.substr(0, pos));
            _dstIP = _dstIP.substr(pos+1);
        }
        in_buf_[8] = 0x00;
        
        async_write(_socket, buffer(in_buf_, 8), // Always 8-byte
             [this, self](boost::system::error_code ec, std::size_t length)
             {
                 if (!ec)
                     do_read_reply();
                 else
                     cout << "SOCKS handshake response write" << ec.message() << endl;
             });
    }


    void do_sock_resolve(){
        auto self(shared_from_this());
//	    cout << "Sock resolving" << endl;
        _sock_resolver.async_resolve(_sock_query,
            [this, self](boost::system::error_code ec,
                         tcp::resolver::iterator endpoint_iterator){
                if (!ec){
                    // Attempt a connection to the first endpoint in the list. Each endpoint
                    // will be tried until we successfully establish a connection.
                    do_connect(endpoint_iterator);
                } else{
                    _socket.close();
                }
            });
    }


    void do_resolve(){
        auto self(shared_from_this());
	//cout << "Resolving" << endl;
        _resolver.async_resolve(_query,
                [this, self](boost::system::error_code ec,
                        tcp::resolver::iterator endpoint_iterator){
                if (!ec){
                    // Attempt a connection to the first endpoint in the list. Each endpoint
                    // will be tried until we successfully establish a connection.
		    _dstIP = endpoint_iterator->endpoint().address().to_string();
		    _dstport = to_string(endpoint_iterator->endpoint().port());
                    do_sock_resolve();
                } else{
                    _socket.close();
                }
        });
    }
    void do_connect(tcp::resolver::iterator endpoint_iterator){
        auto self(shared_from_this());
//	    cout << "connecting" << endl;
        async_connect(_socket,
 			endpoint_iterator,
			 [this, self](boost::system::error_code ec, tcp::resolver::iterator){
            if (!ec){
                send_socks4_request();
            } else{
		        output_shell(0, "connection error");
                _socket.close();
            }
        });
    }

    void do_read_reply() {
        auto self(shared_from_this());
//	    cout << "reading reply" << endl;
        _socket.async_read_some(
                buffer(_data, max_length),
                [this, self](boost::system::error_code ec, size_t length) {
                    if (!ec){
                        do_read();
                    } else{
                        _socket.close();
                    }
                });
    }

    void do_read() {
        auto self(shared_from_this());
//	    cout << "Reading" << endl;
        _socket.async_read_some(
            buffer(_data, max_length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec){
                    string cmd;
                    for (size_t i = 0; i < length; ++i) {
                        cmd += _data[i];
			cout << _data[i];
                    }
                    output_shell(_session, cmd);
                    if (cmd.find("%")!=string::npos)
			        do_send_cmd();
                    do_read();
                } else{
		    cout << "Error" << endl;
                    _socket.close();
                }
        });
    }
    void do_send_cmd(){
        auto self(shared_from_this());
        string line;
        getline(_in, line);
	    line += '\n';
        output_command(_session, line);
        _socket.async_send(
            buffer(line),
            [this, self](boost::system::error_code ec, size_t){
            if (ec){
                _socket.close();
            }
        });
    }
};
class Client{
private:
    string hostname;
    string port;
    string filename;
    string sockhost;
    string sockport;
    int session;
public:
    Client(string hostname_, string port_, string filename_, int session_, string sockhost_, string sockport_){
        hostname = hostname_;
        port = port_;
        filename = filename_;
        session = session_;
        sockhost = sockhost_;
        sockport = sockport_;
    }
    void start(){
        make_shared<ShellSession>(hostname, port, filename, session, sockhost, sockport)->start();
    }
    string output_server(){
        string CSS = R"(            <th scope="col">)";
        CSS += hostname;
        CSS += R"(:)";
        CSS += port;
        CSS += R"(</th>)";
        return CSS;
    }
};

int main(){
    vector <Client> clients;
    string parse_parameter;
    char *tmp = getenv("QUERY_STRING");
    if (tmp != nullptr)
        parse_parameter = tmp;
    else
        parse_parameter = "";    

    regex reg("((|&)\\w+=)([^&]+)");
    regex sockreg("(&)(sh=.+)(&)(sp=.+)");
    smatch m;

    string sockhost, sockport;

    //parse_parameter = "h0=nplinux1.cs.nctu.edu.tw&p0=7777&f0=t1.txt&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&sh=nplinux1.cs.nctu.edu.tw&sp=8888";

    if (regex_search(parse_parameter, m, sockreg)){
        sockhost = m[2].str().substr(3);
        sockport = m[4].str().substr(3);
    }

    parse_parameter = parse_parameter.substr(0, parse_parameter.find(m[0].str()));
  //  cout << parse_parameter << endl;

    int session = 0;
    while (regex_search(parse_parameter, m, reg)){
        string hostname = m[3].str();

        parse_parameter = m.suffix().str();
        regex_search(parse_parameter, m, reg);
        string port = m[3].str();

        parse_parameter = m.suffix().str();
        regex_search(parse_parameter, m, reg);
        string filename = m[3].str();
        parse_parameter = m.suffix().str();

        Client client(hostname, port, filename, session, sockhost, sockport);
        clients.push_back(client);
        ++session;
    }

    string CSS = R"(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>NP Project 3 Console</title>
    <link
      rel="stylesheet"
      href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
      integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
        font-size: 1rem !important;
      }
      body {
        background-color: #212529;
      }
      pre {
        color: #cccccc;
      }
      b {
        color: #ffffff;
      }
    </style>
  </head>
  <body>
    <table class="table table-dark table-bordered">
      <thead>
        <tr>)";
    for (int i = 0; i < session; ++i) {
        CSS += clients[i].output_server();
    }
    CSS+=R"(        </tr>
      </thead>
      <tbody>
        <tr>)";
    for (int i = 0; i < session; ++i) {
        CSS+=R"(            <td><pre id="s)";
	CSS+= to_string(i);
	CSS+=R"(" class="mb-0"></pre></td>)";
    }
    CSS+=R"(        </tr>
      </tbody>
    </table>
  </body>
</html>)";
    cout << "HTTP/1.1 200 OK" << endl;
    cout << "Content-type:text/html" << endl << endl;
    cout << CSS;
	
    try {
        for (unsigned int i = 0; i < clients.size(); ++i) {
            clients[i].start();
        }
        global_io_service.run();
    } catch (exception& e){
        cout << "Error: " << e.what() << endl;
    }
}
