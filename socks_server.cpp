#include <cstdlib>
#include <iostream>
#include <string>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <fstream>
#include <vector>
#include <sys/wait.h>
#include <sys/types.h>
#include <sstream>

#define Accept 0x5A
#define Reject 0x5B

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;

io_service global_io_service;

void childHandler(int signo){
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
    // NON-BLOCKING WAIT
    // Return immediately if no child has exited.
}

class SockSession : public enable_shared_from_this<SockSession>{
public:
	SockSession(tcp::socket in_socket):
	in_socket_(move(in_socket)),
	out_socket_(global_io_service),
	resolver(global_io_service),
    acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), 0)),
    _in("socks.conf"),
	in_buf_(max_length),
	out_buf_(max_length){}

	void start()
	{
	    cout << "Start connection" << endl;
        global_io_service.notify_fork(io_service::fork_prepare);
        pid_t pid = fork();
        if (pid == 0){
            global_io_service.notify_fork(io_service::fork_child);
            read_socks_handshake();
        } else{
            global_io_service.notify_fork(io_service::fork_parent);
            in_socket_.close();
        }
	}

private:
    enum { max_length = 4096 };
	enum Mode {Connect, Bind};
	enum Protocol {SOCKS4, SOCKS4A};
    tcp::socket in_socket_;
    tcp::socket out_socket_;
    tcp::resolver resolver;
    ip::tcp::acceptor acceptor;
    ifstream _in;

    Mode mode;
    Protocol protocol;
    bool permission;
    string S_IP;
    string S_port;
    string remote_host_information_;
    unsigned short remote_port_information_;
    unsigned char remote_host_[4];
    unsigned char remote_port_[2];
    vector<unsigned char> in_buf_;
    vector<unsigned char> out_buf_;

    void printInformation(Mode mode, string &S_IP, string &S_PORT){
        cout << "<S_IP>: " << S_IP << endl;
        cout << "<S_PORT>: " << S_PORT << endl;
        cout << "<D_IP>: " << remote_host_information_ << endl;
        cout << "<D_PORT>: " << remote_port_information_ << endl;
        if (mode == Connect)
            cout << "<Command>: CONNECT" << endl;
        else
            cout << "<Command>: BIND" << endl;
        if (permission)
            cout << "<Reply>: Accept" << endl;
        else
            cout << "<Reply>: Reject" << endl;
    }

    bool checkIP(string IP){
        string hostIP = remote_host_information_;
        for (int i = 0; i < 4; ++i) {
            size_t pos, pos2;
            string tmp, tmp2;
            if (i!=3){
                pos = IP.find('.');
                tmp = IP.substr(0, pos);
                pos2 = hostIP.find('.');
                tmp2 = hostIP.substr(0, pos);
            } else{
                tmp = IP;
                tmp2 = hostIP;
            }

            if (tmp != tmp2 && tmp != "*")
                return false;

            if (i != 3){
                IP = IP.substr(pos+1);
                hostIP = hostIP.substr(pos2+1);
            }
        }
        return true;
    }

    bool firewall(){
        string line;
        while (getline(_in, line)){
            istringstream iss(line);
            string permit, mode, IP;
            iss >> permit >> mode >> IP;

            if ( mode == "c" && in_buf_[1] == 0x01 && checkIP(IP) ){
                return true;
            } else if( mode == "b" && in_buf_[1] == 0x02 && checkIP(IP) ){
                return true;
            } else{
                continue;
            }
        }
        return false;
    }

    bool domain(){
        for (int i = 0; i < 3; ++i) {
            if (in_buf_[4+i] != 0x00)
                return false;
        }
        return in_buf_[7] != 0x00;
    }
    string findDomain(){
        string Domain;
        unsigned int pos=0;
        for (unsigned int i = 8; i < in_buf_.size(); ++i) {
            if (in_buf_[i] == 0x00){
                pos = i;
                break;
            }
        }
        //cout << "pos:" << pos << endl;
        if (pos!=0){
            for (unsigned int i = pos+1; i < in_buf_.size()-1; ++i) {
                Domain += in_buf_[i];
            }
        }
        return Domain;
    }

	void read_socks_handshake()
	{
		auto self(shared_from_this());

		in_socket_.async_receive(boost::asio::buffer(in_buf_),
			[this, self](boost::system::error_code ec, std::size_t length)
			{
			if (!ec)
               		{ 
                    
                    if (in_buf_[0] != 0x04)
                    {
                        cout << "SOCKS request is invalid. Closing session." << endl;
                        return;
                    }

                    for (int i = 0; i < 2; ++i) {
                        remote_port_[i] = in_buf_[2+i];
                    }
                    remote_port_information_ = remote_port_[0] * 256 + remote_port_[1];
                    for (int i = 0; i < 4; ++i) {
                        remote_host_[i] = in_buf_[4+i];
                    }
                    S_IP = in_socket_.remote_endpoint().address().to_string();
                    S_port = to_string(in_socket_.remote_endpoint().port());

                    if (domain()){
			            //cout << "I need to find domain" << endl;
                        remote_host_information_ = findDomain();
                        protocol = SOCKS4A;
                    }
                    else{
                        protocol = SOCKS4;
			            //cout << "Not found domain" << endl;
                        for (int i = 0; i < 4; ++i) {
                            unsigned short val = remote_host_[i];
			            //cout << val << ".";
                            remote_host_information_ += to_string(val);
                            if (i!=3)
                                remote_host_information_ += '.';
                        }
			            //cout << endl;
                    }

                    permission = firewall();

                    if (in_buf_[1] == 0x01){ // Connect Mode
                        mode = Connect;
                        in_buf_[0] = 0x00;
                        if (permission)
                            in_buf_[1] = Accept;
                        else
                            in_buf_[1] = Reject;

                        for (int i = 2; i < 8; ++i) {
                            in_buf_[i] = 0x00;
                        }
                    } else if (in_buf_[1] == 0x02){ // Bind Mode
                        mode = Bind;
                        unsigned int bind_port = acceptor.local_endpoint().port();

                        in_buf_[0] = 0x00;
                        if (permission)
                            in_buf_[1] = Accept;
                        else
                            in_buf_[1] = Reject;

                        in_buf_[2] = bind_port / 256;
                        in_buf_[3] = bind_port % 256;

                        for (int i = 4; i < 8; ++i) {
                            in_buf_[i] = 0x00;
                        }
                    } else{
                        cout << "Error CD" << endl;
                        in_socket_.close();
                    }
                    do_resolve();
				}
				else
					cout << "SOCKS handshake request" << ec.message() << endl;
			});
	}

    /*
     * SOCKS4_REPLY packet (VN=0, CD=90(accepted) or 91(rejected or failed))
     * +----+----+----+----+----+----+----+----+
     * | VN | CD | DSTPORT | DSTIP |
     * +----+----+----+----+----+----+----+----+
     * bytes: 1 1 2 4
     *
     * */

	void do_bind(){
        auto self(shared_from_this());
        //	cout << "Binding ..." << endl;
        acceptor.async_accept(out_socket_, [this,self](boost::system::error_code ec) {
            if(!ec){
                unsigned int bind_port = acceptor.local_endpoint().port();
                in_buf_[0] = 0x00;
                in_buf_[1] = Accept;

                in_buf_[2] = bind_port / 256;
                in_buf_[3] = bind_port % 256;

                for (int i = 4; i < 8; ++i) {
                    in_buf_[i] = 0x00;
                }
                send_socks_reply();
                do_resolve();
            }else{
                cout << ec.message() << endl;
            }
        });
    }

    void send_socks_reply()
    {
        //	cout << "send reply" << endl;
        auto self(shared_from_this());

        async_write(in_socket_, boost::asio::buffer(in_buf_, 8), // Always 8-byte
             [this, self](boost::system::error_code ec, std::size_t length)
             {
                 if (ec){
                     cout << "reply fail" << endl;
                     cout << "SOCKS handshake response write" << ec.message() << endl;}
             });
    }

    void do_resolve(){
        auto self(shared_from_this());
        tcp::resolver::query _query(tcp::v4(), remote_host_information_, to_string(remote_port_information_));
        resolver.async_resolve(_query,
            [this, self](boost::system::error_code ec,
                         tcp::resolver::iterator endpoint_iterator){
                if (!ec){
                    // Attempt a connection to the first endpoint in the list. Each endpoint
                    // will be tried until we successfully establish a connection.
                    do_connect(endpoint_iterator);
                } else{
                    cout << "failed to resolve " << remote_host_information_ << ":" << remote_port_information_ << endl;
                    cout << ec.message() << endl;
                }
            });
    }

	void do_connect(tcp::resolver::iterator endpoint_iterator)
	{
		auto self(shared_from_this());

		ip::tcp::endpoint endpoint = *endpoint_iterator;

        remote_host_information_ = endpoint.address().to_string();
        remote_port_information_ = endpoint.port();

        if (protocol == SOCKS4A){
            int pos = 0, offset=4;
            for (size_t i = 0; i < remote_host_information_.length(); ++i) {
                if (offset == 7){
                    in_buf_[offset] = (unsigned char) atoi(remote_host_information_.substr(pos).c_str());
                    break;
                }
                if (remote_host_information_[i] == '.'){
                    in_buf_[offset] = (unsigned char) atoi(remote_host_information_.substr(pos, i).c_str());
                    pos = i+1;
                    ++offset;
                }
            }
        }

        printInformation(mode, S_IP, S_port);
        send_socks_reply();

        if (mode == Bind){
            do_bind();
        }

        async_connect(out_socket_, endpoint_iterator,
			[this, self](const boost::system::error_code& ec, tcp::resolver::iterator)
			{
				if (!ec)
				{
					do_read(3);
				}
				else
				{
					cout << "failed to connect " << remote_host_information_ << ":" << remote_port_information_ << endl;
					cout << ec.message() << endl;
				}
			});
	}

	void do_read(int direction){
		//cout << "Reading ..." << endl;
		auto self(shared_from_this());

		// We must divide reads by direction to not permit second read call on the same socket.
		if (direction & 0x1)
			in_socket_.async_receive(boost::asio::buffer(in_buf_),
				[this, self](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
						do_write(1, length);
					else //if (ec != boost::asio::error::eof)
					{
						cout << "closing session. Client socket read error" << ec.message() << endl;
						// Most probably client closed socket. Let's close both sockets and exit session.
						in_socket_.close();
						out_socket_.close();
					}

				});

		if (direction & 0x2)
			out_socket_.async_receive(boost::asio::buffer(out_buf_),
				[this, self](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						do_write(2, length);
					}
					else //if (ec != boost::asio::error::eof)
					{
						cout << "closing session. Remote socket read error" << ec.message();
						// Most probably remote server closed socket. Let's close both sockets and exit session.
						in_socket_.close();
						out_socket_.close();
					}
				});
	}

	void do_write(int direction, size_t Length)
	{
		//cout << "Writing" << endl;
		auto self(shared_from_this());

		switch (direction)
		{
		case 1:
			boost::asio::async_write(out_socket_, boost::asio::buffer(in_buf_, Length),
				[this, self, direction](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
						do_read(direction);
					else
					{
						cout << "closing session. Client socket write error" << ec.message() << endl;
						// Most probably client closed socket. Let's close both sockets and exit session.
						//in_socket_.close();
						//out_socket_.close();
					}
				});
			break;
		case 2:
			boost::asio::async_write(in_socket_, boost::asio::buffer(out_buf_, Length),
				[this, self, direction](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
						do_read(direction);
					else
					{
						cout << "closing session. Remote socket write error" << ec.message();
						// Most probably remote server closed socket. Let's close both sockets and exit session.
						//in_socket_.close();
						//out_socket_.close();
					}
				});
			break;
        default:
            cout << "Error" << endl;
            break;
		}
	}
};

class SockServer
{
public:
	SockServer(unsigned short port)
		: acceptor_(global_io_service, tcp::endpoint(tcp::v4(), port)),
		in_socket_(global_io_service)
	{
		do_accept();
	}

private:
	void do_accept()
	{
		acceptor_.async_accept(in_socket_,
			[this](boost::system::error_code ec)
			{
				if (!ec)
					make_shared<SockSession>(move(in_socket_))->start();
				else
					cout << "socket accept error" << ec.message() << endl;

				do_accept();
			});
	}

	tcp::acceptor acceptor_;
	tcp::socket in_socket_;
};

int main(int argc, char* const argv[])
{
	try
	{
		if (argc != 2)
		{
			cout << "Usage: socks_server <port>" << endl;
			return 1;
		}

		signal(SIGCHLD, childHandler);

		unsigned short port = atoi(argv[1]);
				
		SockServer server(port);
		global_io_service.run();
	}
	catch (std::exception& e)
	{
		cout << "Error: " <<  e.what() << endl;
	}

	return 0;
}
