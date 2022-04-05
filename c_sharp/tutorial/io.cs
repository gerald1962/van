using System;

namespace IO {
	class Program {
		static void Main(String[] args) {
			string  cmd, n_str;
			int  n;
			
			/* Ouput the request. */
			Console.WriteLine("Enter command:");

			/* The user can input his command, which is stored in
			 * the variable command.  */
			cmd = Console.ReadLine();

			/* Print the value of the varialbe cmd, which will
			 * display the input value. */
			Console.WriteLine("The command name is: " + cmd);

			/* You cannot implicitly convert type 'string' to
			 * 'int'. */
			Console.WriteLine("Enter the value:");
			n_str = Console.ReadLine();
			n = Convert.ToInt32(n_str);
			Console.WriteLine("The command value is: " + n);			
		}
	}
}
