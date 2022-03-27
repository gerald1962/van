using System;

/* Data Type	Size	 Description
 * int	        4 bytes	 Stores whole numbers from -2,147,483,648 to
 *                       2,147,483,647
 * long	        8 bytes	 Stores whole numbers from -9,223,372,036,854,775,808 to
 *                       9,223,372,036,854,775,807
 * float	4 bytes  Stores fractional numbers. Sufficient for storing 6 to 
 *                       7 decimal digits
 * double	8 bytes	 Stores fractional numbers. Sufficient for storing 15
 *                       decimal digits
 * bool	        1 bit	 Stores true or false values
 * char	        2 bytes	 Stores a single character/letter, surrounded by single
 *                       quotes
 * string	2 bytes  per character. Stores a sequence of characters,
 *                       surrounded by double quotes */
namespace Variable {
	class Program {
		static void Main(string[] args) {
			int i;
			long l;
			float f;
			double d;
			char c;
			bool b;
			string s1, s2, s3;
			
			i = 15;
			Console.WriteLine("int = {0}", i);

			l = 15000000000L;
			Console.WriteLine("long = {0}", l);

			f = 3.14F;
			Console.WriteLine("float = {0}", f);
			
			d = 3.14D;
			Console.WriteLine("double = {0}", d);

			c = 'D';
			Console.WriteLine("char = {0}", c);

			b = true;
			Console.WriteLine("bool = {0}", b);
			
			s1 = "John";
			Console.WriteLine("string = {0}", s1);

			/* You can also use the + character to add a variable to
			 * another variable: */
			s1 = "John ";
			s2 = "Doe";
			s3 = s1 + s2;
			Console.WriteLine("add string = {0}", s3);
		}
	}
}
