include /usr/local/etc/PcapPlusPlus.mk

CPP_FLAG=-std=c++17
OPT_FLAG = -O3
SHM_FLAG = -lrt
all:

# multi-score
	g++ $(CPP_FLAG) $(PCAPPP_BUILD_FLAGS) $(PCAPPP_INCLUDES) -c -o main_multicore.o main_multicore.cpp $(SHM_FLAG)
	g++ $(CPP_FLAG) $(OPT_FLAG) -o dyso_multicore.o dyso_multicore.cpp $(SHM_FLAG)

# pcpp compile
	g++ $(CPP_FLAG) $(PCAPPP_LIBS_DIR) -static-libstdc++ -o pcpp_dyso.o main_multicore.o $(PCAPPP_LIBS) $(SHM_FLAG)

# Clean Target
clean:
	rm main_multicore.o
	rm pcpp_dyso.o
	rm dyso_multicore.o