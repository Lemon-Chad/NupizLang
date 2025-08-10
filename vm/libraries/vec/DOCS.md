
# NPVec Documentation

Fast C++ vector library for Nupiz.

- [vec](#vec)
- [at](#at)
- [size](#size)
- [append](#append)
- [insert](#insert)
- [remove](#remove)
- [pop](#pop)

## vec

`vec(*args)`

Returns a vector with all of the arguments as elements.

## at

`at(vector, index)`

Returns the element at the given index of the vector.

## size

`size(vector)`

Returns the length of the given vector.

## append

`append(vector, obj)`

Appends the given element to the end of the given vector.

## insert

`insert(vector, obj, index)`

Inserts the given element at the index provided in the vector, shifting all elements including the element already at the index to the right.

## remove

`remove(vector, index)`

Removes the element at the given index and shifts all elements to the left.

## pop

`pop(vector)`

Removes the last element in the vector and returns it.
