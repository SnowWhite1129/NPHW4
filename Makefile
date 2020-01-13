all:
	g++ -Wall -std=c++11 socks_server.cpp -o socks_server -lboost_system -lboost_thread -lpthread
	g++ -Wall -std=c++11 console.cpp -o hw4.cgi -lboost_system -lboost_thread -lpthread

