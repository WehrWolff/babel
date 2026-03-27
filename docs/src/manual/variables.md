# Variables

A variable allows you to store a value and associate a name with it. It's useful when you want to reuse a value later in your program (for example after some math). Variables are perfect to avoid repetition, you can let the compiler remember the data for you instead of writing it out everywhere yourself. A constant is a special form of variable, unlike normal variables constants cannot be changed.

## Declaring Variables

Variables need to be declared before they can be used. To declare a variable use the `let` keyword, if you want a constant instead use `const`:

```ts
let x: int = 5
const PI: float = 3.14159265358
```

The type annotations in the above example can be omitted, the compiler is smart enough to figure it out on its own:

```ts
let x = 5
const PI = 3.14159265358
```

Once you’ve declared a constant or variable of a certain type, you can’t declare it again with the same name, or change it to store values of a different type. Nor can you change a constant into a variable or a variable into a constant.

If a stored value in your code won’t change, always declare it as a constant. Use variables only for storing values that change.

!!! note
    You may have heard about constness vs. immutability. In some languages like JavaScript those actually aren't interchangeable (constants can still be mutated through methods). In Babel however they are synonyms, if a variable is declared `const` it will never mutate.

## Reusing Values in Variables

One reason we use variables is that they allow us to easily reuse values in different parts of our code.

When a value is reused many times, it will appear in multiple places in the program. Re-typing that value becomes tedious, and it increases the chance of making mistakes. Also, without a descriptive name, the meaning of the value may be unclear.

Consider the following piece of code:
```cpp
print(299792458)
print(299793458 * 2)
print(299792458 / 1000)
```

Instead of writing the same value again and again, we can declare it as a variable (in this case as a constant). To reuse the value we simply use the name we gave the variable:
```ts
const SPEED_OF_LIGHT = 299792458
print(SPEED_OF_LIGHT)
print(SPEED_OF_LIGHT * 2)
print(SPEED_OF_LIGHT / 1000)
```

You might wonder: "What's the point of a variable if the name is longer than the value?"

A variable provides several benefits. First, a descriptive name helps communicate the intent better. A raw value, often called a magic value, gives you no hint about what it represents. Secondly, using values directly throughout your code is known as hardcoding. If that value ever needs to change, you must hunt it down and update it in every place it appears. With a variable, you have to change the value only once in its declaration.

Finally, you might have noticed a typo in the first example. Of course you can also make those when writing a variable name, but the difference is important: A mistaken literal is still a valid number, so your program will produce the wrong result. In contrast, if you misspell a variable name, the compiler will complain because the mistyped name was never declared. This helps you achieve your intended goal.

Also, using a variable will not make your program slower, the compiler will optimize it for you.

## Changing the Value of a Variable

You can change the value of an existing variable to a new value of a compatible type:

```ts
let x = 5
x = 10
```

However the value of a constant cannot be changed after it's set, attempting to do so will yield a compiler error:
```ts
const PI = 3.14159265358
PI = 3 // error: Cannot assign to constant 'PI'
```

## Naming Variables

Variable names must start with a letter (A-Z or a-z) or an underscore. Numbers (0-9) are also allowed in variables, however they may not appear at the start. Babel has special keywords like `let` or `const`, which are reserved and disallowed as variable names. Variables are case-sensitive, for example `name` and `NAME` are different variables. 
By convention variables names are in camel case: If you have variables that are hard to read because they are actually composed from multiple words, start every new word with an uppercase letter, like `playerHealth`. Constants are in uppercase entirely, insert an underscore in hard to read areas: `MAX_HEALTH_POINTS`.
