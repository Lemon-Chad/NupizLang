
# IOFile Documentation

Rudimentary high level file IO library for Nupiz.

- [openFile](#openfile)
- [closeFile](#closefile)
- [readFile](#readfile)
- [fileLength](#filelength)
- [writeFile](#writefile)
- [writeFileAt](#writefileat)
- [writeFileByte](#writefilebyte)

## openFile

`openFile(path, mode)`

Returns a file object for the given file path, opened with the provided mode. Mode is a C-style mode flag, such as `r`, `w+`, `a+b`, etc.

## closeFile

`closeFile(file)`

Closes the given file. Returns true if the file was closed, and false if the file was already closed.

## readFile

`readFile(file)`

Reads the entirety of the given file as a string and returns it.

## fileLength

`fileLength(file)`

Returns the size of the given file.

## writeFile

`writeFile(file, obj)`

Writes the given object as a string to the given file and returns the number of bytes written.

## writeFileAt

`writeFileAt(file, obj, index)`

Writes the given object as a string to the given file at the given index and returns the number of bytes written.

## writeFileByte

`writeFileByte(file, byte)`

Writes the given number to the given file as a byte.
