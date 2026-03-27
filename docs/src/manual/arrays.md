# Arrays

So far, we have worked with single values stored in variables. However, programs often need to handle multiple values of the same kind, especially when working with structured or grouped data. 

An array is a collection of values of the same type stored sequentially in memory. Each value in an array is called an _element_, and every element can be accessed using its position, called an _index_. Arrays also have a static size, meaning the number of elements is determined when the array is created and cannot change afterwards. This means that arrays are somewhat limited, as elements can neither be added nor removed. In the future we will cover data structures that allow this.

Arrays are useful when you want to store multiple pieces of related data in a single variable. For example, you might use an array to store the red, green, and blue values of a color, or the x, y, and z coordinates of a point in space.

## Creating Arrays

Here is how you would create an array:

```ts
let arr = new Array(1, 2, 3)
```

In the code above we use the `new` keyword to construct an array with size 3 and the integer elements 1, 2 and 3. This can be seen more explicitly from the type signature, altough most of the time we omit the type signature, as it's already evident from the constructor `new Array`:

```ts
let arr: Array<int, 3> = new Array(1, 2, 3)
```

## Accessing Array Elements

To access array elements you can use the subscript operator, just place the index in between square brackets, after the name of the array:

```ts
let arr = new Array(1, 2, 3)
print(arr[0]) // prints 1
```

!!! note
    As you saw above we used the index zero. Indexing always starts at zero, the first element has that index. The second element has index one and so on. If you're new to programming this might be a bit confusing. There are two reasons this is done:
        
    1. This maps more closely to how computers work.
    2. Because of this basically every other programming language does the same, so it's for compatibility reasons as well.

We can also change the elements:

```ts
let arr = new Array(1, 2, 3)
arr[0] = 4
arr[1] = 5
arr[2] = 6
// now the array is 4, 5, 6
```

The same cannot be done if the array is declared `const`:

```ts
const arr = new Array(1, 2, 3)
arr[0] = 4 // error: The underlying array is constant
```
