/* The using System line means that you are using the System library in your
 * project. Which gives you some useful classes and functions like Console class
 * or the WriteLine function/method. */
using System;

/* OOP stands for Object-Oriented Programming.
 * Procedural programming is about writing procedures or methods that perform
 * operations on the data, while object-oriented programming is about creating
 * objects that contain both data and methods:
 * class:  fruit
 * object: apple
 * Object-oriented programming has several advantages over procedural
 * programming:
 * OOP is faster and easier to execute
 * OOP provides a clear structure for the programs
 * OOP helps to keep theode DRY "Don't Repeat Yourself", and makes the code
 * easier to maintain, modify and debug OOP makes it possible to create full
 * reusable applications with less code and shorter development time. */

/* Van OS. */
namespace Class {
	class Prg {
		static void Main(string[] args) {
			/* Spefify a Van OS thread. */
			Vos.Thd thd_1;

			/* To create a Van OS thread object, specify the class
			 * name, followed by the object name, and use the keyword new: */
			thd_1 = new Vos.Thd("thd_1");

			/* Print the current state of the Van OS thread. */
			thd_1.Trace();

			/* Suspend the Main() thread. */
			thd_1.main_wait.WaitOne();
		}
	}	
}
