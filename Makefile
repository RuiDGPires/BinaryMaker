TARGET = txttobin
FLAGS =
fin =
fout =

default: debug

$(TARGET): txttobin.c
	gcc $(FLAGS) -pthread $^ -o $(TARGET)

debug: FLAGS += -O0 -Wall -g -D DEBUG
debug: $(TARGET)

release: FLAGS += -O3
release: $(TARGET)

run:
	./$(TARGET) $(fin) $(fout)

clean:
	rm -f $(TARGET)
