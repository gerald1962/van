/* Using System means that we can use classes from the System namespace. */
using System;

/* COM Interop - Interoperational - is a technology included in the . NET
 * Framework Common Language Runtime (CLR) that enables Component Object Model
 * (COM) objects to interact with . NET objects, and vice versa. COM Interop
 * aims to provide access to the existing COM components without requiring that
 * the original component be modified.
 *
 * InteropServices provides a wide variety of members that support COM interop
 * and platform invoke services: e.g.
 * DllImportAttribute indicates that the attributed method is exposed by an
 * unmanaged dynamic-link library (DLL) as a static entry point. */
using System.Runtime.InteropServices;

/* namespace is used to organize your code, and it is a container for classes
 * and other namespaces. */
namespace Libc {
	class Program {
		/* Use DllImport to import the libc test function incr(). */
		[DllImport("libc.so", EntryPoint="incr")]
		static extern int incr(int i);

		/* Another thing that always appear in a C# program, is the Main
		 * method. */
		 static void Main(string[] args) {
			 int i, rv;

			 /* Initialize the test integer. */
			 i = 0;

			 /* Call incr() using platform invoke. */
			 rv = incr(i);

			 /* Console is a class of the System namespace, which has
			 * a WriteLine() method that is used to output/print
			 * text.  */
			 Console.WriteLine("libc: incr({0}) -> {1}", i, rv);
		 }
	}
}
