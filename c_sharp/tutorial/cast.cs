using System;

/* Type casting is when you assign a value of one data type to another type.
 * There are two types of casting.
 * Implicit Casting (automatically) - converting a smaller type to a larger type
 * size: char -> int -> long -> float -> double
 * Explicit Casting (manually) - converting a larger type to a smaller size
 * type: double -> float -> long -> int -> char 
 * It is also possible to convert data types explicitly by using built-in
 * methods, such as Convert.ToBoolean, ... */
namespace Cast {
	class Program {
		static void Main(string[] args) {
			/* Implicit casting is done automatically when passing a
			 * smaller size type to a larger size type: */
			int i = 9;
			double d = i;
			bool b;
			string s;
			
			Console.WriteLine("i = {0}, d = {1}", i, d);
			
			/* Explicit casting must be done manually by placing the
			 * type in parentheses in front of the value: */
			d = 3.14;
			i = (int) d;
			Console.WriteLine("i = {0}, d = {1}", i, d);

			b = true;
			s = Convert.ToString(b);
			Console.WriteLine("b = {0}", s);
		}
	}
}
