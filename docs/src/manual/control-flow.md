# Control Flow

So far, our programs have executed statements one after another, from top to bottom. However, most programs need to make decisions or repeat actions based on certain conditions.

Control flow determines the order in which code is executed. It allows a program to choose between different paths, skip parts of code, or repeat sections multiple times.

In this section, we will explore how to control the flow of a program using conditions and loops.

## The if-Statement

An `if`-statement is used for conditional evaluation, it executes other statements only when a condition is `TRUE`. Here is how you could write one:

```lua
let number = 10
if number > 0 then
    print("The number is positive!")
end
```

First comes the `if` keyword, after that the condition, in our case whether the number is greater than zero. To tell the compiler which statements to execute when the condition is `TRUE` we put them between the `then` and `end` keyword. So the code above would display "The number is positive!". If we changed number to e.g. `-10`, nothing would happen instead. This is because now the condition would be `FALSE`, so the code between `then` and `end` is skipped.

!!! note
    The indentation inside the `if`-block is optional, however by convention and to make code more readable it is strongly recommended.

### Operators used in conditions

For the condition you can use all of the comparison and boolean operators (you can read about them again in the section on operators). However it is worth talking about the boolean operators again. The boolean type (that is the "`TRUE` or `FALSE`"-type) supports chaining, meaning you can connect multiple conditions using an and or an or. For this you should almost always use `&&` and `||` respectively, instead of the `&` and `|` variants.

The advantage is that the first two use what's called short-circuit evaluation. With short-circuit evaluation, expressions are evaluated from left to right, and evaluation stops as soon as the result is known.

For an `&&` expression, if the left side is `FALSE`, the entire condition must also be `FALSE`, so the right side is not evaluated.
For an `||` expression, if the left side is `TRUE`, the entire condition is already `TRUE`, so again the right side is skipped.

This is not only more efficient, but can also prevent errors. Consider the following example:

```lua
if n != 0 && number % n == 0 then
    print("number is evenly divisible by n")
end
```

Here, we first check that `n` is not zero. We evaluate the second condition, only if this is `TRUE`. That's important, because dividing by zero is not allowed. If `n` were `0`, the first condition would already be `FALSE`, and the second part would never be evaluated -- preventing an error.

### if-else

Often we don't just want to execute code if a condition is `TRUE`, but also do something different if it's `FALSE`. Of course you could use another `if`-statement with the opposite condition, but there is actually a more idiomatic and readable way to do this, using an `else` block:

```lua
let number = 11
if number % 2 == 0 then
    print("the number is even")
else
    print("the number is odd")
end
```

If the condition is `TRUE`, the first block is executed. Otherwise, the `else` block runs. Since `11 % 2` is equal to `1` the condition is `FALSE` and the `the number is odd` is printed.

Instead of ending the `if` block immediately with `end`, it is extended by the `else` branch. This continues the control flow and introduces another block. The entire construct is then finalized with a single `end`.

### Multiple Conditions with elif

Sometimes you want to check more than just two cases. For this, you can use `elif` (short for “else if”) to chain multiple conditions:

```lua
let score = 85

if score >= 90 then
    print("grade: A")
elif score >= 80 then
    print("grade: B")
elif score >= 70 then
    print("grade: C")
elif score >= 60 then
    print("grade: D")
else
    print("grade: F")
end
```

Each `elif` introduces a new condition and its own block. Instead of closing the previous block with `end`, the statement continues with the next `elif` or `else`. Only after the final branch is the entire construct closed with a single `end`.

The conditions are evaluated from top to bottom. As soon as one condition evaluates to `TRUE`, its corresponding block is executed and all remaining conditions are skipped.

In this example, only `"grade: B"` is printed. Even though `score >= 70` and `score >= 60` are also `TRUE`, they are never checked because an earlier condition already matched.

## Loops

In many programs, you will want to execute the same piece of code multiple times. Writing the same statements over and over again would be tedious and error-prone. Instead, you can use a loop to repeat a block of code automatically.

### The while-loop

The simplest form of a loop is the `while` loop. It repeatedly executes a block of code as long as a given condition is `TRUE`:

```ruby
let waterTemperature = 20
let heaterOn = TRUE

while heaterOn do
    print("Heating...")
    waterTemperature += 5
    if waterTemperature >= 100 then
        heaterOn = FALSE
    end
end

print("Reached boiling point")
```

The loop starts by evaluating the condition. If it is `TRUE`, the code inside the loop is executed. After that, the condition is checked again. This process repeats until the condition becomes `FALSE`, at which point the loop stops.

In the example above, the loop runs until the water reaches its boiling point at 100°C (if you use Fahrenheit [here is a map](https://commons.wikimedia.org/wiki/File:Countries_that_use_Celsius.svg) of how wrong you are).

### The for-loop

A `for` loop is closely related to a `while` loop, but it allows you to specify an initialization and update instruction along with the the condition:

```ruby
for i := 0; i < 10; i++ do
    print(i)
end
```

This loop consists of three parts, separated by semicolons[^1]:

1. **Initialization:** `i := 0`\
    This is executed once at the beginning of the loop and is typically used to declare and initialize a loop variable.
2. **Condition:** `i < 10`\
    Before each iteration, this condition is checked. If it is `TRUE`, the loop continues; otherwise, it stops.
3. **Update:** `i++`\
    After each iteration, this expression is executed. In this case, it increases `i` by `1`.

[^1]: This is one of the few places you will see semicolons in the Babel source code. Refer to the section on semicolons for more info.

We can use a loop similar to the one above to iterate through collections, like arrays:

```ruby
let arr = new Array(79, 50, 40, 99, 22, 32, 96, 7)
for i := 0; i < arr.length; i++ do
    print(arr[i])
end
```

The loop above would print all the array elements in order. But notice how we actually don't care about the index `i`, we just use it to obtain the elements. When looping through collections when we are just interested in the elements we can use a `for`-`in` loop instead:

```ruby
let arr = new Array(79, 50, 40, 99, 22, 42, 96, 7)
for elmnt in arr do
    print(elmnt)
end
```
