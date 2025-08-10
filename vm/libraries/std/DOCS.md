
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

`asByte(char)`

Takes a character and returns it as a byte.
