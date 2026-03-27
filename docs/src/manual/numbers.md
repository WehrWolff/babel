# Integer and Floating-Point Numbers

Numbers appear in programs not only through variables and calculations, but also directly as literals written in the source code. Babel supports several forms of integer and floating-point literals, this section describes how numeric literals are written, which formats are allowed, and how Babel interprets them.

## Integers

Integers are whole numbers with no fractional component, such as `42` and `-23`. Their maximum and minimum value depends on their size (the number of bits used to store values). The integer types include their size in their names — for example, the type int8 represents an 8-bit integer.

To improve readability, you may use apostrophes as digit separators, e.g. `8'388'608` is the same as `8388608`.

By default integer literals are interpreted as a decimal number. You can however write them in binary, octal or hexadecimal by using the `0b`, `0o` or `0x` prefixes respectively:

```ts
const decimalInteger = 67
const binaryInteger = 0b1000011
const octalInteger = 0o103
const hexadecimalInteger = 0x43
// all of them are equal to 67
```

By default integer literals have a size of 32 bits, however you can write a suffix, should you require a different size. In the table below you will find all available suffixes for integers:

| Suffix   | Suffix Meaning | Corresponding Type |
|----------|----------------|--------------------|
| `B`, `b` | byte           | int8               |
| `S`, `s` | short          | int16              |
| `I`, `i` | integer        | int32              |
| `L`, `l` | long           | int64              |
| `C`, `c` | cent           | int128             |

!!! note
    To better separate the suffix from the rest of the literal, you may include an underscore in front of it. In hexadecimals however they are required, so they can be differentiated from the letters that may appear in them.

To sum up, here are a few examples:

```cpp
const a = 1'234
const b = 42L // 42 as an int64
const c = 123'456'789_C // 123456789 as an int128
const d = 0xFF
const e = 0xdead'beef_L // the underscore here is required
const f = 0b1010
const g = 0b1111'0000s
const h = 0o755

const invalid = 1''234 // error: adjacent digit separators
```

## Floating-Point Numbers

Floating-point numbers have a fractional component, such as `3.14159`, `0.1`, and `-273.15`. Babel provides a variety of floating-point types that support different sizes of numbers, just like it has different sizes of integers.

Floating-point numbers let you work with very small and very large numbers, but can’t represent every possible value in that range. Unlike integer calculations, which always produce an exact result, floating-point math might need to round results. The default rounding mode is nearest with ties to even. This means round to the closest representable value and in case of a tie, round towards the one with an even least significant bit. For example, when storing the number `10'000` as a float32, the next largest number you can represent is `10'000.001` –– values between these two numbers round to one or the other. The space between numbers is also variable; there are larger spaces between large numbers than between small numbers. For example, the next float32 value after `0.001` is `0.0010000002`, which is smaller than the spacing after `10'000`.

!!! note
    Because of this you should be aware that exact comparisons between floats are a bad idea: You might think that `0.1 + 0.2 == 0.3` should be true, however the rounding can foil your plans and `0.1 + 0.2` is actually equal to `0.30000000000000004`. This is not a bug or something that only occurs in the Babel programming language, your computer is to blame for this one. If you want to better understand this, maybe the following videos can help: 
    
    - [https://www.youtube.com/watch?v=PZRI1IfStY0](https://www.youtube.com/watch?v=PZRI1IfStY0)
    - [https://www.youtube.com/watch?v=bbkcEiUjehk](https://www.youtube.com/watch?v=bbkcEiUjehk)

If you need the same spacing between all possible values, or if the calculations you’re doing require exact results and don’t call for the special values listed above, a floating-point number might not be the right data type. Consider using fixed-point numbers instead: To represent currency, instead of using imprecise floats like `1.95`, you could use an integer that represents it in cents (e.g. `195`).

### Advanced floating-point syntax

Floating-points support the scientific [e-notation](https://en.wikipedia.org/wiki/Scientific_notation#E_notation), e.g. `1e6` or `2.5E-4`. In case of hexadecimal floats (with prefix `0x`) p is used instead of e and signifies "times two to the power of", e.g. `0x1.8p4` or `0x4p-1`.

By default floating-point literals have a size of 32 bits, like with integers you may write a suffix if you require a different size. Also any valid integer literal becomes a floating-point literal if followed by one of these suffixes:

| Suffix   | Suffix Meaning | Corresponding Type |
|----------|----------------|--------------------|
| `H`, `h` | half           | float16            |
| `F`, `f` | float          | float32            |
| `D`, `d` | double         | float64            |
| `Q`, `q` | quadruple      | float128           |

Again here are some examples:

```cpp
const fa = 0.25
const fb = .5 // you may omit the 0 in front of the dot
const fc = 12.345d // 12.345 as a float32
const fe = 3.14_q // 3.14 as a float128 (underscore is optional)
const ff = 10f // 10 as a float32
const fg = 0x10_d // underscore is required (hex value)
const fh = 0xF.fp3 // 15.9375 * 8 = 127.5
```

### Special floating-point values

There are three specified standard floating-point values that do not correspond to any point on the real number line:

| Symbol   | Name              | Description                                           |
|----------|-------------------|-------------------------------------------------------|
| `Inf`    | positive infinity | a value greater than all finite floating-point values |
| `-Inf`   | negative infinity | a value less than all finite floating-point values    |
| `NaN`    | not a number      | not equal to any floating-point, result of e.g. 0/0   |

All of them can include a type suffix with leading underscore, for example `Inf_d` is positive infinity for the float64 type.
