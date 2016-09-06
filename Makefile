CXX=g++-5
CC=gcc-5
CXXFLAGS+=-std=c++14 -Wall -Wextra -g -ggdb -I . 
CFLAGS+= -Wall -Wextra -g -ggdb -I . -std=c99




OBJS=src/att.o src/uuid.o src/bledevice.o src/att_pdu.o src/pretty_printers.o src/blestatemachine.o src/float.o src/logging.o


lescan: $(OBJS) lescan.o
	$(CXX) -o $@ $^ -lbluetooth

temperature: $(OBJS) temperature.o
	$(CXX) -o $@ $^ -lbluetooth

blelogger: $(OBJS) blelogger.o
	$(CXX) -o $@ $^ -lbluetooth

prog: $(OBJS) bluetooth.o
	$(CXX) -o $@ $^ -lbluetooth

.PHONY: clean

clean:
	rm -f $(OBJS)
