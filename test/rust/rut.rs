// Rust tutorial.

fn eg_number_systems() {
}

fn eg_suffix_types() {
    let a = 3.0f32;
    let b = 42i8;

    println!("suffix-types-1: f32: a = {}", a);
    println!("suffix-types-2: i8:  b = {}", b);
}

fn eg_types() {
    let a: i8 = 20;
    let b: bool = false;
    let c: char = 'a';
    let d: f32 = 4.4321;

    println!("types-1: i8:   a = {}", a);
    println!("types-2: bool: b = {}", b);
    println!("types-3: char: c = {}", c);
    println!("types-4: f32:  d = {}", d);
    println!("");
}

const PI: f32 = 3.141;

fn eg_i32() {
    println!("i32-1: from {} to {}", i32::min_value(), i32::max_value());
    println!("");
}

fn eg_8() {
    println!("8-1 i8: from {} to {}", i8::min_value(), i8::max_value());
    println!("8-2 u8: from {} to {}", u8::min_value(), u8::max_value());
    println!("");
}

fn eg_const() {
    println!("const-1: PI = {}", PI);
    println!("");
}

fn eg_mut() {
    let mut a;

    a = 12;
    println!("mut-1: a = {}", a);

    a = 33;
    println!("mut-2: a = {}", a);
    println!("");
}

fn eg_shadowing() {
    let a;

    a = 12;
    println!("shadowing-1: a = {}", a);

    let a = 33;
    println!("shadowing-2: a = {}", a);
    println!("");
}

fn eg_variable() {
    let a;

    a = 12;
    println!("variable-1: a = {}", a);
    println!("");
}

fn main() {
    println!("Hello Gerald!");
    println!("");

    eg_variable();
    eg_shadowing();
    eg_mut();
    eg_const();
    eg_8();
    eg_i32();
    eg_types();
    eg_suffix_types();
    eg_number_systems();
}
