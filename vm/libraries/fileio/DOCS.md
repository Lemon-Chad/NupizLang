
# IOFile Documentation

`import iofile;`

Rudimentary high level file IO library for Nupiz.

- [openFile](#openfile)
- [closeFile](#closefile)
- [readFile](#readfile)
- [fileLength](#filelength)
- [writeFile](#writefile)
- [writeFileAt](#writefileat)
- [writeFileByte](#writefilebyte)
- [getFileDirectory](#getfiledirectory)
- [getAbsPath](#getabspath)
- [getCWD](#getcwd)
- [setCWD](#setcwd)
- [fileExists](#fileexists)
- [dirExists](#direxists)

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

## getFileDirectory

`getFileDirectory(path)`

Takes in a file path and returns the path to the directory in which the file is stored.

## getAbsPath

`getAbsPath(path)`

Takes in a path and returns the absolute path from the system root to the file location.

## getCWD

`getCWD()`

Returns the program's current working directory.

## setCWD

`setCWD(path)`

Takes in a directory path and sets the program's current working directory to the given path.

## fileExists

`fileExists(path)`

Takes in a file path and returns whether or not the file exists.

## dirExists

`dirExists(path)`

Takes in a directory path and returns wheter or not the directory exists.
