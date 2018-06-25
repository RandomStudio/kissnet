#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <csignal>
#include <kissnet.hpp>

namespace kn = kissnet;

int main(int argc, char* argv[])
{

	//Configuration (by default)
	kn::port_t port = 12321;
	//If specified : get port from command line
	if(argc >= 2)
	{
		port = (kn::port_t)strtoul(argv[1], nullptr, 10);
	}

	//We need to store thread objects somewhere:
	std::vector<std::thread> threads;
	//We need to store socket objects somewhere
	std::vector<kn::socket<kn::protocol::tcp>> sockets;

	//Create a listening TCP socket on requested port
	kn::socket<kn::protocol::tcp> listen_socket({ "0.0.0.0", port });
	listen_socket.bind();
	listen_socket.listen();

	//close program upon ctrl+c or other signals
	std::signal(SIGINT, [](int) {
		std::cout << "Got sigint signal...\n";
		std::exit(0);
	});

	//Send the SIGINT signal to ourself if user press return on "server" terminal
	std::thread run_th([] {
		std::cout << "press return to close server...\n";
		std::cin.get(); //This call only returns when user hit RETURN
		std::cin.clear();
		std::raise(SIGINT);
	});

	//Let that thread run alone
	run_th.detach();

	//Loop that continously accept connections
	while(true)
	{
		std::cout << "Waiting for a client on port " << port << '\n';
		sockets.emplace_back(listen_socket.accept());
		auto& sock = sockets.back();

		//Create thread that will echo bytes received to the client
		threads.emplace_back([&](kn::socket<kn::protocol::tcp>& sock) {
			//Internal loop
			bool ok = true;
			//Static 1k buffer
			kn::buffer<1024> buff;

			//While connection is alive
			while(ok)
			{
				//attept to receive data
				auto [size, valid] = sock.recv(buff);

				//If socket is still valid
				if(valid)
				{
					//no bytes = cleanly disconnected
					if(size == 0)
						ok = false;
					else
						sock.send(buff.data(), size);
				}
				//If not valid remote host closed conection
				else
				{
					ok = 0;
				}
			}

			//Now that we are outside the loop, erase this socket from the "sokets" list:
			std::cout << "detected disconnect\n";
			auto me_iterator = std::find(sockets.begin(), sockets.end(), std::ref(sock));
			if(me_iterator != sockets.end())
			{
				std::cout << "closing socket...\n";
				sockets.erase(me_iterator);
			}
		},
							 std::ref(sock));
		threads.back().detach();
	}

	return 0;
}
