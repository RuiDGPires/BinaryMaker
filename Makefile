TARGET = binarymaker 
FLAGS =
fin =
fout =

default: release

$(TARGET): binarymaker.c
	gcc $(FLAGS) -pthread $^ -o $(TARGET)

debug: FLAGS += -O0 -Wall -g -D DEBUG
debug: $(TARGET)

release: FLAGS += -O3
release: $(TARGET)

run:
	@echo
	@./$(TARGET) $(fin) $(fout)
	@hexdump $(fout)

clean:
	rm -f $(TARGET)
