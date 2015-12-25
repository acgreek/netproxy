#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/from_stream.hpp>

class Connection  {
    public:
        Connection(boost::asio::io_service &io_service) : socket_(io_service) {};
        boost::asio::ip::tcp::socket  & socket() {return socket_;}
        boost::asio::streambuf& buffer() {return buffer_;};
    private:
        boost::asio::streambuf buffer_;
        boost::asio::ip::tcp::socket socket_;
};
class Proxy : public boost::enable_shared_from_this<Proxy>{
    public:
        Proxy (boost::asio::io_service &io_service) : from_(io_service),to_(io_service) {};
        void start(std::string &host, std::string &port) {
             BOOST_LOG_TRIVIAL(info) << "received connection from " << from_.socket().remote_endpoint().address().to_string() << ":" << from_.socket().remote_endpoint().port();
            boost::asio::ip::tcp::resolver resolver (to_.socket().get_io_service());
            const boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve({host.c_str(), port.c_str()});
            boost::asio::async_connect(to_.socket(), endpoint_iterator , boost::bind(&Proxy::handle_remote_connect, shared_from_this(), boost::asio::placeholders::error));
        }
        boost::asio::ip::tcp::socket & from_socket() {
            return from_.socket();
        }
        void handle_remote_connect(const boost::system::error_code& error) {
            if (error) {
                BOOST_LOG_TRIVIAL(info) << "remote connection error " << connDescription_;
                cancelAll();
                return;
            }
            std::stringstream ss;
            ss << from_.socket().remote_endpoint().address().to_string() << ":" << from_.socket().remote_endpoint().port() << " to " << to_.socket().remote_endpoint().address().to_string() << ":" << to_.socket().remote_endpoint().port();
            connDescription_ = ss.str();
            BOOST_LOG_TRIVIAL(info) << "remote connection established from " << connDescription_ ;
            setupLocalRead();
            setupRemoteRead();
        }

        void setupLocalRead(){
            BOOST_LOG_TRIVIAL(trace) << "setup local read "  << connDescription_;
            boost::asio::streambuf::mutable_buffers_type mutableBuffer = from_.buffer().prepare(1024);
            from_.socket().async_read_some(mutableBuffer,boost::bind(&Proxy::handleLocalRead, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }

        void handleLocalRead(const boost::system::error_code& error, size_t bytes){
            if (error) {
                BOOST_LOG_TRIVIAL(info) << "local connection error " << connDescription_ << " :" << error.message();
                cancelAll();
                from_.socket().close();
                return;
            }
            BOOST_LOG_TRIVIAL(trace) << "read " << bytes << " from local connection "  << connDescription_;
            from_.buffer().commit(bytes);
            boost::asio::async_write(to_.socket(), from_.buffer(), boost::bind(&Proxy::handleLocalWrite, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }

        void handleLocalWrite(const boost::system::error_code& error, size_t bytes){
            if (error) {
                BOOST_LOG_TRIVIAL(info) << "write local error " <<  connDescription_ << " :" << error.message();
                cancelAll();
                to_.socket().close();
                return;
            }
            setupLocalRead();
        }

        void setupRemoteRead(){
            BOOST_LOG_TRIVIAL(debug) << "setup remote read " << connDescription_;
            boost::asio::streambuf::mutable_buffers_type mutableBuffer = to_.buffer().prepare(1024);
            to_.socket().async_read_some(mutableBuffer, boost::bind(&Proxy::handleRemoteRead, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));
        }

        void handleRemoteRead(const boost::system::error_code& error, size_t bytes){
            if (error) {
                BOOST_LOG_TRIVIAL(info) << "read remote error "  <<  connDescription_ << " :" << error.message();
                cancelAll();
                to_.socket().close();
                return;
            }
            BOOST_LOG_TRIVIAL(trace) << "read " << bytes << " from remote connection " <<connDescription_;
            to_.buffer().commit(bytes);
            boost::asio::async_write(from_.socket(), to_.buffer(), boost::bind(&Proxy::handleRemoteWrite, shared_from_this(), boost::asio::placeholders::error ,boost::asio::placeholders::bytes_transferred));

        }
        void handleRemoteWrite(const boost::system::error_code& error, size_t bytes) {
            if (error) {
                BOOST_LOG_TRIVIAL(info) << "write remote error " <<  connDescription_ << " :" << error.message() ;
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
        std::string connDescription_;
};

class Server{
    public:
        Server(boost::asio::io_service &io_service, int port, std::string &remoteHost, std::string remotePort) :remoteHost_(remoteHost),remotePort_(remotePort), acceptor_(io_service,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) ) {

        }
        void start() {
            new_session_.reset(new Proxy(acceptor_.get_io_service()));
            BOOST_LOG_TRIVIAL(trace) << "acceptor listening on " << acceptor_.local_endpoint().address().to_string() << ":" << acceptor_.local_endpoint().port();
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
                BOOST_LOG_TRIVIAL(error) << "accept error " << error.message();
            }
        }
        std::string remoteHost_;
        std::string remotePort_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::shared_ptr<Proxy> new_session_;
};

int main(int argc, char * argv[]) {
    unsigned short listenPort = 9000;
    std::string remoteHost= "127.0.0.1";
    std::string remotePort = "9001";
    std::string loggingSettingsFile;
    // Declare the supported options.
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("port,p", boost::program_options::value<unsigned short>(&listenPort)->default_value(9000), "listen port")
        ("remoteport,P", boost::program_options::value<std::string>(&remotePort)->default_value("9001"), "remote port to connect to")
        ("remotehost,H", boost::program_options::value<std::string>(&remoteHost)->default_value("127.0.0.1"), "remote host to connect to")
        ("logsettings,l", boost::program_options::value<std::string>(&loggingSettingsFile)->default_value("logging.ini"), "path and file name of logging settings file")
        ;
    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }
    std::ifstream file(loggingSettingsFile.c_str());
    if (file.good()) {
        boost::log::init_from_stream(file);
    }
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
