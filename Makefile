CC=g++
CFLAGS=-c -g -O0
LDFLAGS=-ll
SOURCES=ts_script.tab.o ts_script_lex.o ts_script.o ts_script_codegen.o
BIN=ts_script

all: $(BIN) 

$(BIN): $(SOURCES)
	g++ $(LDFLAGS) -o $@ $(SOURCES)

ts_script.o: ts_script.tab.o ts_script_lex.o ts_script_codegen.o
	g++ -o $@ $(CFLAGS) ts_script.cpp

ts_script_codegen.o:
	g++ -o $@ $(CFLAGS) ts_script_codegen.cpp

ts_script.tab.o: ts_script.tab.cpp
	$(CC) -o $@ $(CFLAGS) ts_script.tab.cpp

ts_script_lex.o: ts_script_lex.cpp
	$(CC) -o $@ $(CFLAGS) ts_script_lex.cpp
	
ts_script.tab.cpp:
	bison -o $@ -d ts_script.y 	

ts_script_lex.cpp:
	flex -o $@ ts_script.l

clean:
	rm -rf $(SOURCES) ts_script
	rm -rf ts_script.tab.* ts_script_lex.cpp
	rm -rf parse_tree.dot plugin.c
