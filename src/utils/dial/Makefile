.PHONY: all clean
.DEFAULT: all

all: server

server: example.cpp dialserver.cpp dialserver.h upnpserver.cpp upnpserver.h \
        socketserver.cpp socketserver.h mcastserver.cpp mcastserver.h \
        dialapplication.cpp dialapplication.h Makefile
	g++ -Wall -Wextra $(filter %.cpp, $^) -o $@ -Wl,-lpthread

clean:
	$(RM) server
