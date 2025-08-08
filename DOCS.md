
# Nupiz Documentation

The following documentation is very rough and intended to serve as a starting point until a *readthedocs* page is created.

***Static typing is not currently implemented on this compiler, so any instances of static typing in the documentation are yet to be implemented.***

## Table of Contents

### [Library Documentation](vm/libraries/DOCS.md)

### Language Documentation

1. [Build Instructions](#build-instructions)
1. [Compiler Usage](#compiler-usage)
1. [Variables](#variables)
1. [Conditionals](#conditionals)
1. [Loops](#loops)
1. [Functions](#functions)
1. [Classes](#classes)
1. [Imports](#imports)

## Build Instructions

The virtual machine and compiler can be built by running `make all` under the `vm` directory. This will output an executable at `build/npz`.

## Compiler Usage

`npz [options]`

**Options**:
- `-c [target]` Compile target file
- `-o [target]` Output compiled binary to target file
- `-r [target]` Run target binary file
- `-v` Prints the current version stamp
- `-h` Displays a help message

*Ex:* `npz -c ./main.npz -o ./main.nux -r ./main.nux`

## Variables

Variables can be declared using three keywords, `const`, `let`, or `var`. 

`const` and `var` require explicit typing, while `let` infers.

`const` variables cannot be modified.

```nupiz
const x: num = 4;
var y: str = "Hello!";

let z = true;
```

## Conditionals

```nupiz
if (2 + 3 <= 4) {
    println("Wrong.");
} else if (2 + 3 == 4) {
    println("Wrong again!");
} else {
    println("Correct!");
}
```

## Loops

### For

```nupiz
for (let i = 0; i < 5; i += 1) {
    println("i: " + asString(i));
}
```

### While

```nupiz
let i = 0;
while (i < 5) {
    println("i: " + asString(i));
    i += 1;
}
```

### Iterator (TODO)

```nupiz
let lst = [1, 2, 3];
from (let ele <- lst) {
    println(ele);
}
```

## Functions

Functions are defined with the `func` keyword. Each parameter requires explicit typing, alongside the return type.

```nupiz
func add(x: num, y: num) -> num {
    return x + y;
}
```

Functions can return functions of their own.

```nupiz
func addCurry(x: num) -> num(num) {
    func lambda(y: num) {
        return x + y;
    }

    return lambda;
}
```

Anonymous functions to be added.

## Classes

Classes are defined with the `class` keyword followed by the class name and body. 

Class attributes can be declared the same as a variable.

The constructor method can be declared with the `build` keyword, followed by the body and typed parameters.

Methods can be declared with the `func` keyword, just as a function would. To override a default function, such as `string` or `eq`, use the `def` keyword after `func`.

Methods and attributes are subject to access modifiers, which can be placed after the declaring keyword. Access modifiers include `prv`, `pub`, and `static`.

- `prv` makes the attribute/method private and only accessible from inside of the class's own context. It cannot be accessed from the outside or with `this`.
- `pub` makes the attribute/method public and accessible from any context.
- `static` gives ownership of the attribute/method to the class itself, and not the instances. `static` must come before `pub` or `prv`.

By default, methods are public and attributes are private. Access modifiers cannot be applied to `def` methods.

```nupiz
class Person {
  let prv name: str;
  let prv age: num;

  build(_name, _age) {
    name = _name;
    age = _age;
  }

  func pub sayHello() {
    println("Hello! My name is " + name + ". I am " + 
              asString(age) + " years old.");
  }

  func pub getName() -> str {
    return name;
  }

  func pub getAge() -> num {
    return age;
  }

  func def string() -> str {
    return this.getName() + " (" + 
        asString(this.getAge()) + ")";
  }
}

var jack = Person("Jack", 19);

// Since the string method is overridden,
// this will print "Jack (19)".
println(jack);
```

## Imports

Libraries can be imported with the `import` keyword followed by the library name.  The library will be automatically assigned to a variable of the same name as the library.

Files can be imported with the `import` keyword followed by a string to the file path. File imports will not be automatically be assigned, and the `import` statement will return the namespace as a value to be assigned.

A namespace's variables can be unpacked into the global space with the `unpack` keyword, removing the need to use an accessor on the namespace every time.

```nupiz
// desktop/myproject/lib.npz

import std;

func printSum(x: num, y: num) -> void {
    std.println(asString(x + y));
}
```
```nupiz
// desktop/myproject/main.npz

const lib = import "./lib.npz";

// Prints 4.
lib.printSum(1, 3);

unpack lib;

// Prints 9.
// No need for lib.printSum, as lib
// was unpacked.
printSum(2, 7);
```

