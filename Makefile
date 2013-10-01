CXXFLAGS+=-std=c++11 -Wall -Wextra -g -ggdb

prog: bluetooth.o att.o lib/uuid.o
	$(CXX) -o $@ $^ -lbluetooth

bluetooth.o:att.h att_pdu.h bluetooth.cc
