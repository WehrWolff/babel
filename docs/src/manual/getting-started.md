# Getting started

Traditionally, the first program in any new programming language is one that displays the words “Hello world” on the screen. In Babel, this can be done using the `print` function. Here’s how you’d write it:

```python
print("Hello world")
# Prints "Hello world" without a newline
```

This syntax should feel familiar if you've worked with other languages. In Babel, this line of code is a complete program — no need for complex setups or boilerplate. There's no `main()` task required to start, but if you prefer to explicitly define one, you can do so to mark the entry point of your program for larger projects.

```ruby
task main(argc: Int, argv: vList<String>) => void!
    if argc > 1 then
        println() <| fmt() <| "Greetings {argv.join(', ')}!"
    else
        println() <| "Greetings Universe!"
    end
end
```

Whoa, there's a lot going on here now! This is just to show experienced programmers what is possible in Babel. We’ll cover all these concepts — from tasks, argument handling, the pipe operator, and string formatting — in upcoming sections. Don't worry if this seems like a lot to take in right now!