# Data Types

To build programs, you need to work with information, and information always has a form. Types classify that information. A type describes what kind of data a value holds and what operations can be performed on it. Babel is a statically typed language, meaning that the type of every value must be known at compile time. This does not mean you must annotate everything yourself: the compiler performs type inference in most situations, so explicit annotations are usually optional. It does mean that every value has a type, whether you wrote it down or not. Types matter because they tell the compiler how to store data efficiently, but just as importantly, they tell you which operations make sense. Numbers can be squared, while strings cannot.

!!! note
    This section only covers the types itself, if you want to know more about how to use values of certain types, visit the corresponding pages.

## Primitive Types

Primitive types are the most basic building blocks of data in Babel. A primitive type is a concrete type whose data consists of plain old bits. They represent simple values such as integers, floating-point numbers, booleans, and characters. Below you can find a table of primitive types:

| Name     | C Equivalent | Description                                              |
|----------|--------------|----------------------------------------------------------|
| int8     | int8_t       | signed 8-bit integer                                     |
| int16    | int16_t      | signed 16-bit integer                                    |
| int32    | int32_t      | signed 32-bit integer                                    |
| int64    | int64_t      | signed 64-bit integer                                    |
| int128   | __int128     | signed 128-bit integer                                   |
| float16  | (none)       | 16-bit floating point (10-bit mantissa)                  |
| float32  | float        | 32-bit floating point (23-bit mantissa)                  |
| float64  | double       | 64-bit floating point (52-bit mantissa)                  |
| float128 | (none)       | 128-bit floating point (112-bit mantissa)                |
| bool     | bool         | either `TRUE` or `FALSE`                                 |
| char     | uint8_t      | unsigned 8-bit integer, represents a symbol              |
| void     | void         | used as return type only, indicates no value is returned |

There are also the `int` and `float` type, which default to `int32` and `float32` respectively. They are exactly the same type and intended only to save you some typing.

## Aggregate Types

Aggregate types combine multiple values into a single, structured whole. Unlike primitive types, aggregates expose internal components that can be accessed or modified individually. They allow you to organize related pieces of data together, making programs easier to understand, maintain, and extend. Aggregates exist to help you build more expressive abstractions, and model real-world entities more naturally than primitives alone could.

### Arrays

An array arranges elements sequentially in memory. Essentially they act like a collection of other elements. The type of an array contains its inner type: the type of its elements. It also contains the size of the array: an array of 3 integers is not the same as an array of 4 integers.

### Strings

Strings are sequences of characters (chars), it's actually the same as an array of characters. However it is special in the sense that they are used differently in real world situations.
