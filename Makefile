CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic

APP := simulator
ORACLE_TEST := oracle_tests

.PHONY: all run_app test test_differential run_d5 run_all clean

all: $(APP) $(ORACLE_TEST)

# main.cpp currently includes the selected structure's .cpp file.
# OracleQueue.cpp is compiled separately.
$(APP): app/main.cpp app/ITriageQueue.h oracle/OracleQueue.cpp oracle/OracleQueue.h
	$(CXX) $(CXXFLAGS) app/main.cpp oracle/OracleQueue.cpp -o $(APP)

$(ORACLE_TEST): oracle/oracle_tests.cpp oracle/OracleQueue.cpp oracle/OracleQueue.h app/ITriageQueue.h
	$(CXX) $(CXXFLAGS) oracle/oracle_tests.cpp oracle/OracleQueue.cpp -o $(ORACLE_TEST)

run_app: $(APP)
	./$(APP)

test: $(ORACLE_TEST)
	./$(ORACLE_TEST)

test_differential:
	python oracle/run_differential.py

run_d5:
	python adversary/run_experiments.py

run_all: all test run_app run_d5

clean:
	$(RM) $(APP) $(APP).exe $(ORACLE_TEST) $(ORACLE_TEST).exe
