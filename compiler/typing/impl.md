
## First Pass (Parsing) [x]

Types will be read, but unresolved, and stored as such.

Unresolved types are composed of a `path`, their generic `args` and a `nullable` boolean.

For example: `monads.Box<string>?`

`path`: `[monads, Box]`

`args`: `[UnresolvedType(string)]`

`nullable`: `true`

## Second Pass (Package Collector) [ ]

This pass will generate a tree of type packages for all root nodes. Type packages will contain a `path`, a `types` hashmap, and a `children` hashmap.

Files that wish to export their types will need to have a defined `package path`, with the new `pack` statement.

```nupiz
pack lemon.monad;

// The Box<T> type will be
// defined under lemon.monad.Box
class Box<T> {
    ...
}
```

```
        lemon
     /    |    \
  monad nupiz  web
        /   \   |
   compiler vm api
```

This will generate a directed graph of packages and a directed graph of types. Each package will have the types underneath them, so they can be accessed with a path lookup.

Any types that depend on so-far undefined types will be added to a queue. After all types are defined as far as possible, the queue will backpatch any undefined types. Any types that are still undefined will raise an error.

## Third Pass (Type Machine) [ ]

The final pass will simulate the AST to check for type mismatches. No code will be evaluated, but the tree will be walked in entirety with a psuedo-global+local variable storage system to store the types of variables. 

Expressions will be ensured to type safe, and any unsafe expressions will be raised as errors.
