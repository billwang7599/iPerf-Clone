CXX = g++
CXXFLAGS = -Wall -g

main: iperfer.cpp
	$(CXX) $(CXXFLAGS) iperfer.cpp -o iPerfer

clean:
	rm -f iPerfer
