#If you use threads, add -pthread here.
COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare

#Any libraries you might need linked in.
#LINKLIBS = -lm

#The components of each program. When you create a src/foo.c source file, add obj/foo.o here, separated
#by a space (e.g. SOMEOBJECTS = obj/foo.o obj/bar.o obj/baz.o).
HTTPSERVEROBJECTS = obj/http_server.o
HTTPCLIENTOBJECTS = obj/http_client.o
SERVEROBJECTS = obj/server.o
CLIENTOBJECTS = obj/client.o
TALKEROBJECTS = obj/talker.o
LISTENEROBJECTS = obj/listener.o


#Every rule listed here as .PHONY is "phony": when you say you want that rule satisfied,
#Make knows not to bother checking whether the file exists, it just runs the recipes regardless.
#(Usually used for rules whose targets are conceptual, rather than real files, such as 'clean'.
#If you DIDNT mark clean phony, then if there is a file named 'clean' in your directory, running
#`make clean` would do nothing!!!)
.PHONY: all http clean


#The first rule in the Makefile is the default (the one chosen by plain `make`).
http : obj http_server http_client
all : obj server client talker listener


#$@: name of rule's target
#$^: the entire dependency string (after expansions)
#CC is a built in variable for the default C compiler; it usually defaults to "gcc". (CXX is g++).
http_server: $(HTTPSERVEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

http_client: $(HTTPCLIENTOBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

server: $(SERVEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

client: $(CLIENTOBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

talker: $(TALKEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

listener: $(LISTENEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)


#RM is a built-in variable that defaults to "rm -f".
clean :
	$(RM) obj/*.o http_server http_client server client talker listener

#$<: the first dependency in the list
#The % sign means "match one or more characters". You specify it in the target, and when a file
#dependency is checked, if its name matches this pattern, this rule is used. You can also use the %
#in your list of dependencies, and it will insert whatever characters were matched for the target name.
obj/%.o: src/%.c
	$(CC) $(COMPILERFLAGS) -c -o $@ $<
obj:
	mkdir -p obj
