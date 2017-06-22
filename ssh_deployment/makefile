CIBLE = deployment
SRCS =  Deployment.cpp AskingHost.cpp

CC = g++
CPP = g++

FLAGS = -std=c++11 -Wall -g -pthread -O3

CFLAGS = $(FLAGS)
CXXFLAGS = $(FLAGS)

OBJS = $(SRCS:.cpp=.o)

$(CIBLE): $(OBJS)
	$(CPP) $(LDFLAGS) -o $(CIBLE) $(OBJS)
	rm -f *~ $(OBJS)
clean:
	rm -f  *~  $(CIBLE) $(OBJS)

deployment.o: Deployment.cpp AskingHost.h
