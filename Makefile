TARGET = txttobin
FLAGS =

default: debug

$(TARGET): txttobin.c
	gcc $(FLAGS) -pthread $^ -o $(TARGET)

debug: FLAGS += -O0 -Wall -g -D DEBUG
debug: $(TARGET)

release: FLAGS += -O3
release: $(TARGET)

clean:
	rm -f $(TARGET)
