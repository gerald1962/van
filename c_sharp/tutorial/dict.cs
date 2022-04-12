using System;

/* Include the dictionary. */
using System.Collections.Generic;

namespace Dict {
	class P {
		static Dictionary<string, object> param;

		static void Main() {
			/* Dictionary is a generic collection which is generally
			 * used to store key/value pairs: declare a Dictionary
			 * containing just the type object and then cast your
			 * results. */
			param = new Dictionary<string, object>();
			string  s;
			int     i;
			bool    b;
			
			/* Adding key/value pairs in the Dictionary using Add() method. */
			param.Add("name", "x");
			param.Add("count", 0);
			param.Add("cond",  true);

			/* Use the key as index to get the value, but then you
			 * need to cast the results. */
			s = (string) param["name"];
			i = (int)    param["count"];
			b = (bool)   param["cond"];
			
			Console.WriteLine("dict: s = {0}, i = {1}, b = {2}",
					  s, i, b);

			/* Use the key as index to change the value. */
			param["name"]  = "y";
			param["count"] = +1;;
			param["cond"]  = false;
			
			s = (string) param["name"];
			i = (int)    param["count"];
			b = (bool)   param["cond"];
			
			Console.WriteLine("dict: s = {0}, i = {1}, b = {2}",
					  s, i, b);
		}
	}
}
