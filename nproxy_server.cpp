#include <iostream>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

class Connection  {
    public:
        Connection(boost::asio::io_service &io_service) : socket_(io_service) {};
        boost::asio::ip::tcp::socket  & socket() {return socket_;}

        void start() {
            std::cout << "connected\n";
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
        void start() {
            std::cout << "local connection\n";
            boost::asio::ip::tcp::resolver resolver (to_.socket().get_io_service());
            std::cout << " 1 local connection\n";
            const boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve({"127.0.0.1", "9001"});
            std::cout << " 2 local connection\n";
            boost::asio::async_connect(to_.socket(), endpoint_iterator , boost::bind(&Proxy::handle_remote_connect, shared_from_this(), boost::asio::placeholders::error));
            std::cout << " 3 local connection\n";
        }
        boost::asio::ip::tcp::socket & from_socket() {
            return from_.socket();
        }
        void handle_remote_connect(const boost::system::error_code& error) {
            std::cout << "made remote connection\n";
            boost::asio::async_read(to_.socket(), to_.in(), boost::bind(&Proxy::handleReadRemote, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
            boost::asio::async_read(from_.socket(), from_.in(), boost::bind(&Proxy::handleLocalRemote, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }
        void handleReadRemote(const boost::system::error_code& error, size_t bytes){
            std::cout << "read remote connection\n";
        }
        void handleLocalRemote(const boost::system::error_code& error, size_t bytes){
            std::cout << "read remote connection\n";
        }
    private:
        Connection  from_;
        Connection  to_;
};

class Server{
    public:
        Server(boost::asio::io_service &io_service, int port) : acceptor_(io_service,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) ) {
        }
        void start() {
            new_session_.reset(new Proxy(acceptor_.get_io_service()));
            acceptor_.async_accept(new_session_->from_socket(), boost::bind(&Server::handle_accept, this, new_session_, boost::asio::placeholders::error));
        }
    private:

        void handle_accept(boost::shared_ptr<Proxy> new_session, const boost::system::error_code& error)
        {
            if (!error)  {
                new_session->start();
                new_session.reset(new Proxy(acceptor_.get_io_service()));
                acceptor_.async_accept(new_session->from_socket(), boost::bind(&Server::handle_accept, this, new_session, boost::asio::placeholders::error));
            }
            else {
                std::cerr << "handle accept error " << error << std::endl;
            }
        }
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::shared_ptr<Proxy> new_session_;
};

int main() {
    boost::asio::io_service io_service;
    Server server(io_service, 9000);
    server.start();
    try {
        io_service.run();
    } catch (std::exception & e) {
        std::cerr << "exception : " << e.what() << std::endl;
    }

    return 0;
}
