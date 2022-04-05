/* The using System line means that you are using the System library in your
 * project. Which gives you some useful classes and functions like Console class
 * or the WriteLine function/method. The namespace ProjectName is something that
 * identifies and encapsulates your code within that namespace. */
using System;

/* The Thread class is defined in the System. Threading namespace that must be
 * imported before you can use any threading related types. */
using System.Threading;

/* namespace is used to organize your code, and it is a container for classes
 * and other namespaces. */
namespace Vos {
	/* Everything in C# is associated with classes and objects, along with
	 * its attributes and methods. To create a class, use the class
	 * keyword.
	 * Fields and methods inside classes are often referred to as
	 * "Class Members". 
	 * Thd is a class with i class members: j fields and k methods. */
	class Thd {
		/* The public keyword is called an access modifier, which
		 * specifies that the fields are accessible for other classes. */

		/* Execution states of a Van OS thread. */
		public enum Exec_s { VOS_THD_BOOT, VOS_THD_READY, VOS_THD_KILL,
			VOS_THD_INV };

		/* Initial execution state of a Van OS thread. */
		public Exec_s  exec_s;

		/* Name of a Van OS thread. */
		public string  name;

		/* Reference to a C sharp thread object. 
		 * Once the task assigned to a Thread is completed, that thread
		 * will be terminated and we don’t need to worry about it. */
		public Thread  t; 

		/* A semaphore, that shall suspend the Main() thread. */
		public Semaphore  main_wait;
		
		/* A semaphore, that shall suspend the thread, started by the Main() thread. */
		public Semaphore  this_wait;
		
		/* A new thread shall execute the callback method T_cb(). */
		public void T_cb() {
			Console.WriteLine("T_cb: ...");

			/* The current thread consists of sleeping for about 100
			 * milliseconds. */
			Thread.Sleep(100);
			
			/* Resume the Main() thread. */
			this.main_wait.Release();
		}
		
		/* The C# “this” keyword represents the “this” pointer of a
		 * class or a stuct. The this pointer represents the current
		 * instance of a class or struct. */
		
		/* Print the current state of a thread. */
		public void Trace() {
			Console.WriteLine("{0}: exec_s = {1}", this.name, this.exec_s);
		}

		/* A constructor is a special method that is used to initialize
		 * objects. The advantage of a constructor, is that it is called
		 * when an object of a class is created. It can be used to set
		 * initial values for fields: */
		public Thd(string n) {
			/* Initial the execution state of a Van OS thread. */
			exec_s = Exec_s.VOS_THD_BOOT;

			/* Define the thread name. */
			name = n;

			/* Create a semaphore that can satisfy up to 1
			 * concurrent request. Use an initial count of zero, so
			 * that the entire semaphore count is initially owned by
			 * the Main() thread. */
			main_wait = new Semaphore(0, 1);

			/* Create the a semaphore to control the thread started
			 * by the Main() thread. */
			this_wait = new Semaphore(0, 1);
			
			/* Allocate a C sharp thread object. */
			t = new Thread(new ThreadStart(T_cb));

			/* Start the thread, which shall execute the callback method T_cb(). */
			t.Start();
		}
	}
}
