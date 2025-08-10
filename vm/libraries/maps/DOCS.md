
# NPMap Documentation

`import npmap;`

Fast C++ map library for Nupiz.

- [map](#map)
- [put](#put)
- [emplace](#emplace)
- [get](#get)
- [remove](#remove)
- [has](#has)
- [keys](#keys)

## map

`map(key1, value1, key2, value2, ...)`

Returns a map with the given key and value pairs.

## put

`put(map, key, value)`

Puts the key value pair into the given map, overwriting if it already exists.

## emplace

`emplace(map, key, value)`

Puts the key value pair into the given map assuming it does not exist. Faster than put, but cannot overwrite.

## get

`get(map, key)`

Returns the value for the given key in the map if the pair exists, otherwise errors.

## remove

`remove(map, key)`

Removes the key-value pair associated with the given key if it exists in the map. Returns true if it existed, else false.

## has

`has(map, key)`

Returns whether the key corresponds with a key-value pair in the given map.

## keys

`keys(map, key)`

Returns a [vector](../vec/DOCS.md) containing all of the map's keys.
