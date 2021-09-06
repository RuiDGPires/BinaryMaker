# BinaryMaker

Tool for creating binary files from a text file of hexadecimal numbers

## Example

*a.txt*
```
90 80 10 2B
AA FF FF FF
90 F9 87 02
00 06 F2 C0
```

>binarymaker a.txt b

>hexdump b
```
0000000 8090 2b10 ffaa ffff f990 0287 0600 c0f2
0000010
```
