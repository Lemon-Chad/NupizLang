
# NPVec Documentation

`import npvec;`

Fast C++ vector library for Nupiz.

- [vec](#vec)
- [vecFrom](#vecfrom)
- [at](#at)
- [find](#find)
- [size](#size)
- [append](#append)
- [insert](#insert)
- [remove](#remove)
- [pop](#pop)

## vec

`vec(*args)`

Returns a vector with all of the arguments as elements.

## vecFrom

`vecFrom(obj)`

Returns a vector with all of the elements of the given list or string as the elements of the new vector.

## at

`at(vector, index)`

Returns the element at the given index of the vector.

## find

`find(vector, ele)`

Returns the index of the element in the given vector, or -1 if it is not found.

## size

`size(vector)`

Returns the length of the given vector.

## append

`append(vector, ele)`

Appends the given element to the end of the given vector.

## insert

`insert(vector, ele, index)`

Inserts the given element at the index provided in the vector, shifting all elements including the element already at the index to the right.

## remove

`remove(vector, index)`

Removes the element at the given index and shifts all elements to the left.

## pop

`pop(vector)`

Removes the last element in the vector and returns it.
