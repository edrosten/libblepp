CXXFLAGS+=-std=c++11 -Wall -Wextra -g -ggdb -I . 
CFLAGS+= -Wall -Wextra -g -ggdb -I . 




OBJS=src/att.o src/uuid.o src/logging.o src/bledevice.o src/att_pdu.o src/pretty_printers.o src/blestatemachine.o src/float.o

glove_control: $(OBJS) glove_control.o
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
