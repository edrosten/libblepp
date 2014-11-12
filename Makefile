CXXFLAGS+=-std=c++11 -Wall -Wextra -g -ggdb -I . 
CFLAGS+= -Wall -Wextra -g -ggdb -I . 

prog: bluetooth.o src/att.o lib/uuid.o src/logging.o src/bledevice.o src/att_pdu.o
	$(CXX) -o $@ $^ -lbluetooth

