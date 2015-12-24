#include <iostream>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

bool verbose= false;

class Connection  {
    public:
        Connection(boost::asio::io_service &io_service) : socket_(io_service) {};
        boost::asio::ip::tcp::socket  & socket() {return socket_;}

        void start() {
            if (verbose) std::cout << "connected\n";
        }
        boost::asio::streambuf& in() {return in_;};
        boost::asio::streambuf& out(){return out_;};

    private:
        boost::asio::streambuf in_;
        boost::asio::streambuf out_;
        boost::asio::ip::tcp::socket socket_;
};
class Proxy : public boost::enable_shared_from_this<Proxy>{
    public:
        Proxy (boost::asio::io_service &io_service) : from_(io_service),to_(io_service) {};
        void start(std::string &host, std::string &port) {
            boost::asio::ip::tcp::resolver resolver (to_.socket().get_io_service());
            const boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve({host.c_str(), port.c_str()});
            boost::asio::async_connect(to_.socket(), endpoint_iterator , boost::bind(&Proxy::handle_remote_connect, shared_from_this(), boost::asio::placeholders::error));
        }
        boost::asio::ip::tcp::socket & from_socket() {
            return from_.socket();
        }
        void handle_remote_connect(const boost::system::error_code& error) {
            if (error) {
                std::cout << "remote connection error  :" << error.message()<< "\n";
                cancelAll();
                return;
            }
            if (verbose) std::cout << "remote connection success\n";
            setupLocalRead();
            setupRemoteRead();
        }

        void setupLocalRead(){
            if (verbose) std::cout << "setupLocalRead\n";
            boost::asio::streambuf::mutable_buffers_type mutableBuffer = from_.in().prepare(1024);
            from_.socket().async_read_some(mutableBuffer,boost::bind(&Proxy::handleLocalRead, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }

        void handleLocalRead(const boost::system::error_code& error, size_t bytes){
            if (error) {
                std::cout << "read local error :" << error.message()<< "\n";
                cancelAll();
                from_.socket().close();
                return;
            }
            from_.in().commit(bytes);
            boost::asio::async_write(to_.socket(), from_.in(), boost::bind(&Proxy::handleLocalWrite, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }

        void handleLocalWrite(const boost::system::error_code& error, size_t bytes){
            if (error) {
                std::cout << "write local error :" << error.message()<< "\n";
                cancelAll();
                to_.socket().close();
                return;
            }
            setupLocalRead();
        }

        void setupRemoteRead(){
            if (verbose) std::cout << "setupRemoteRead\n";
            boost::asio::streambuf::mutable_buffers_type mutableBuffer = to_.in().prepare(1024);
            to_.socket().async_read_some(mutableBuffer, boost::bind(&Proxy::handleRemoteRead, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }

        void handleRemoteRead(const boost::system::error_code& error, size_t bytes){
            if (error) {
                std::cout << "read remote error :" << error.message()<< "\n";
                cancelAll();
                to_.socket().close();
                return;
            }
            if (verbose) std::cout << "read remote connection " << bytes << "\n";
            to_.in().commit(bytes);
            boost::asio::async_write(from_.socket(), to_.in(), boost::bind(&Proxy::handleRemoteWrite, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));

        }
        void handleRemoteWrite(const boost::system::error_code& error, size_t bytes) {
            if (error) {
                std::cout << "write remote error :" << error.message()<< "\n";
                cancelAll();
                from_.socket().close();
                return;
            }
            setupRemoteRead();
        }

    private:
        void cancelAll() {
            if (from_.socket().is_open())
                from_.socket().cancel();
            if (to_.socket().is_open())
                to_.socket().cancel();
        }
        Connection  from_;
        Connection  to_;
};

class Server{
    public:
        Server(boost::asio::io_service &io_service, int port, std::string &remoteHost, std::string remotePort) :remoteHost_(remoteHost),remotePort_(remotePort), acceptor_(io_service,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) ) {
        }
        void start() {
            new_session_.reset(new Proxy(acceptor_.get_io_service()));
            acceptor_.async_accept(new_session_->from_socket(), boost::bind(&Server::handle_accept, this, new_session_, boost::asio::placeholders::error));
        }
    private:

        void handle_accept(boost::shared_ptr<Proxy> new_session, const boost::system::error_code& error)
        {
            if (!error)  {
                new_session->start(remoteHost_, remotePort_);
                new_session.reset(new Proxy(acceptor_.get_io_service()));
                acceptor_.async_accept(new_session->from_socket(), boost::bind(&Server::handle_accept, this, new_session, boost::asio::placeholders::error));
            }
            else {
                std::cerr << "handle accept error " << error << std::endl;
            }
        }
        std::string remoteHost_;
        std::string remotePort_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::shared_ptr<Proxy> new_session_;
};

int main() {
    unsigned short listenPort = 9000;
    std::string remoteHost= "127.0.0.1";
    std::string remotePort = "9001";
    boost::asio::io_service io_service;
    Server server(io_service, listenPort, remoteHost, remotePort);
    server.start();
    try {
        io_service.run();
    } catch (std::exception & e) {
        std::cerr << "exception : " << e.what() << std::endl;
    }

    return 0;
}
