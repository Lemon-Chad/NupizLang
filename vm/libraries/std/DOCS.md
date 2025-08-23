
# std Documentation

`import std;`

Standard library for Nupiz.

- [print](#print)
- [println](#println)
- [asString](#asstring)
- [length](#length)
- [append](#append)
- [remove](#remove)
- [pop](#pop)
- [clock](#clock)
- [asByte](#asbyte)
- [cmdargs](#cmdargs)
- [main](#main)
- [slice](#slice)
- [find](#find)
- [split](#split)
- [repeat](#repeat)
- [strtod](#strtod)

## print

`print(*args)`

Takes a variable amount of arguments and prints each separated by a space.

## println

`println(*args)`

Takes a variable amount of arguments and prints each separated by a space followed by a newline at the end.

## asString

`asString(obj)`

Returns any given object in its defined string representation.

## length

`length(obj)`

Returns the length of any measurable collection, such as strings and lists.

## append

`append(list, obj)`

Appends the object to the given list.

## remove

`remove(list, index)`

Removes the element at the given index in the given list.

## pop

`pop(list)`

Removes and returns the final element of the given list.

## clock

`clock()`

Returns the current time as a UNIX timestamp.

## asByte

`asByte(obj)`

Returns the given object in byte form.
    - `character`: Returns the character as a byte.
    - `double`: Returns the number as a list of bytes.

## cmdargs

`cmdargs()`

Returns a list of command line arguments passed to the program.

## main

`main(func)`

Will execute `func(cmdargs)` if the file is currently the target executable.

## slice

`std.slice(string, start, end)`

Returns a copy of the given string from indices `[start:end)`. Accepts negative indices, which start at the null terminator.

## find

`std.find(list, ele)`

Returns the index of the given element in the list. If the element is not in the list, returns -1.

## split

`std.split(string, delimiter)`

Splits a string into a list of substrings each time the delimiter is encountered.

## repeat

`std.repeat(string, count)`

Returns a new string with the given string concatenated in a row as many times as given. Count must be non-negative.

## strtod

`std.strtod(string)`

Parses a string to be a number and returns the number. If the format is invalid, errors.

