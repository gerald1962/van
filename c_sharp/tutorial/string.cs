using System;

/*
 * Strings are used for storing text.
 * A string variable contains a collection of characters surrounded by double
 * quotes: */
namespace Str {
	class Prg {
		static void Main(string[] args) {
			string  src, dest;
			int   n;
			char  c;

			/* Assign a variable of type string a value. */
			src = "Hello";
			Console.WriteLine("String: src = {0}", src);

			/* The length of a string can be found with the Length
			 * property. */
			n = src.Length;
			Console.WriteLine("String: len(\"{0}\") -> {1}", src, n);

			/* The + operator, string.Concat() or string
			 * interpolation $ can be used between strings to
			 * combine them. This is called concatenation: */
			dest = src + " World";
			Console.WriteLine("String: \"{0}\" + \" World\" -> \"{1}\"",
					  src, dest);

			/* You can access the characters in a string by
			 * referring to its index number inside square
			 * brackets []. */
			c = src[0];
			Console.WriteLine("String: src[0] -> '{0}'", c);						
		}
	}
}
