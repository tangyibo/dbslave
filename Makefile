##
##
##
BINDIR= bin
EXES = dbSlaveServer
SRCS=$(wildcard src/*.cpp src/jsoncpp/*.cpp)
OBJS=$(patsubst %.cpp,%.o,$(SRCS) )
RM :=-rm -f 

CFLAGS= -g -Wall -D_GLIBCXX_USE_CXX11_ABI=0 -DVDEBUG=1 -rdynamic 
CXXFLAGS = -g -Wall -D_GLIBCXX_USE_CXX11_ABI=0 -DVDEBUG=1 -rdynamic 
CPPFLAGS = -I./include/mysql/ -I./include/rabbitmq-c -I./include/amqpcpp -I./src
LIBS = -L/usr/local/whistle/mysql/lib/ -L./lib \
       -lmysqlclient -lpthread -lamqpcpp -lrabbitmq -lssl -lcrypto -lboost_signals -lrt

all: dir $(OBJ) $(EXES)

show:
	@echo "EXES=$(EXES)"
	@echo "SRCS=$(SRCS)"
	@echo "OBJS=$(OBJS)"

dir:
	if [ ! -d $(BINDIR) ]; then mkdir $(BINDIR) ; fi;

$(EXES): $(OBJS)
	g++ -o $(BINDIR)/$@ $^ $(CXXFLAGS) $(CPPFLAGS) $(LIBS)

clean:
	$(RM) $(BINDIR)/$(EXES) $(OBJS)
	
.c:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

.cpp:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

#
#

