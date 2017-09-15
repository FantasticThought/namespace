NS_AGENT_OBJS = namespace.o
NS_SERVER_OBJS = server.o

NS_AGENT = namespace
NS_SERVER = server

TARGETS = $(NS_AGENT) $(NS_SERVER)

all: $(NS_AGENT) $(NS_SERVER)

%.o: %.c
	gcc -c $< -o $@

$(NS_AGENT): $(NS_AGENT_OBJS)
	gcc $^ -o $@

$(NS_SERVER): $(NS_SERVER_OBJS)
	gcc $^ -o $@

clean:
	rm -f $(NS_AGENT) $(NS_SERVER) *.o
