OBJS=accounts.o config_file.o dns_resolve.o listener.o log.o \
	mailserv.o options.o pop3_server.o sender.o server.o socket.o \
	thread.o utility.o

TARGET=mailserv

$(TARGET): $(OBJS)
	g++ -o $< $(OBJS)

clean:
	$(RM) $(OBJS) $(TARGET) *~ core
