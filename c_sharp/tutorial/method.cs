using System;

/* MyMethod() is the name of the method.
 * static means that the method belongs to the Program class and not an object
 * of the Program class.
 * void means that this method does not have a return value. 
 *
 * Information can be passed to methods as parameter, see arg.
 * When a parameter is passed to the method, it is called an argument,
 * see "hello".
 *
 * You can also use a default parameter value, by using the equals sign (=). If
 * we call the method without an argument, it uses the default value
 * ("defaul value"): see method_3().
 *
 * Multiple Parameters: you can have as many parameters as you like,
 * see method_4().
 *
 * Return values: the void keyword, used in the examples above, indicates that
 * the method should not return a value. If you want the method to return a
 * value, you can use a primitive data type (such as int or double) instead of
 * void, and use the return keyword inside the method: see method_5().
 *
 * Named arguments: it is also possible to send arguments with the key: value
 * syntax. That way, the order of the arguments does not matter: see method_6().
 *
 * Method overloading: with method overloading, multiple methods can have the
 * same name with different parameters: see method_7(). */
namespace Method {
	class program {
		static void method_7(int i) {
			Console.WriteLine("method_7: i = {0}", i);
		}
		
		static void method_7(string s) {
			Console.WriteLine("method_7: s = {0}", s);			
		}
		
		static void method_7(bool b) {
			Console.WriteLine("method_7: b = {0}", b);
		}
		
		static void method_6(int arg1, int arg2) {
			Console.WriteLine("method_6: arg1 = {0}, args2 = {1}",
					  arg1, arg2);
		}
		
		static int method_5(int i) {
			Console.WriteLine("method_5: i = {0}", i);
			return i + 1;
		}
		
		static void method_4(string s, int i) {
			Console.WriteLine("method_4: s = {0}, i = {1}", s, i);
		}
		
		static void method_3(string arg = "default_value") {
			Console.WriteLine("method_3: arg = \"{0}\"", arg);
		}
		
		static void method_2(string arg) {
			Console.WriteLine("method_2: arg = \"{0}\"", arg);
		}
		
		static void method_1() {
			Console.WriteLine("method_1: no args");
		}

		static void Main(string[] args) {
			int rv;
			
			method_1();
			method_2("Hello");

			method_3("X");
			method_3();

			method_4("Y", 59);

			rv = method_5(0);
			Console.WriteLine("Main:method_5: rv = {0}", rv);

			method_6(arg2: 2, arg1: 1);

			method_7(42);
			method_7("?");
			method_7(false);
		}
	}	
}
