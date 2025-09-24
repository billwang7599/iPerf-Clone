CXX = g++
CXXFLAGS = -Wall -g

main: iPerfer.cpp
	$(CXX) $(CXXFLAGS) iperfer.cpp -o iPerfer

clean:
	rm -f iPerfer
