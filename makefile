CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  lst_timer.cpp http_con.cpp log.cpp sql_connection_pool.cpp  webserver.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

CGISQL.cgi:sign.cpp
	g++ -o ./resources/CGISQL.cgi $^ -lpthread -lmysqlclient

clean:
	rm  -r server
	rm  -r ./resources/CGISQL.cgi
