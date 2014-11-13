CXXFLAGS+=-std=c++11 -Wall -Wextra -g -ggdb -I . 
CFLAGS+= -Wall -Wextra -g -ggdb -I . 

prog: bluetooth.o src/att.o src/uuid.o src/logging.o src/bledevice.o src/att_pdu.o src/pretty_printers.o src/blestatemachine.o src/float.o
	$(CXX) -o $@ $^ -lbluetooth

