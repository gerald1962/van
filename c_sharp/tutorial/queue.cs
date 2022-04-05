/* The using System line means that you are using the System library in your
 * project. Which gives you some useful classes and functions like Console class
 * or the WriteLine function/method. */
using System;

/* The namespace VOS - Van OS - is used to organize its code, and it is a
 * container for VOS classes. Namespace also solves the problem of naming
 * conflict. */
namespace VOS {
	/* Everything in C# VOS is associated with thread or thread input queue
	 * classes and objects, along with its attributes and methods.
	 * T_q_elem stands for thread queue element. */
	class Thread_queue_elem  {
		/* Successor of the thread input queue. */
		public Thread_queue_elem  next;

		/* Test data. */
		public int data;

		/* Print the current state of a VOS thread queue element.
		 * The this keyword refers to the current instance of the 
		 * class. */
		public void Trace() {
			Console.WriteLine("T_q_elem: data = {0}", this.data);
		}

		/* The constructor T_q_elem() is a special method that is used
		 * to initialize the T_q_elem object implicitely associated with
		 * the new VOS.T_q_elem(() method. */
		public Thread_queue_elem(int i) {
			data = i;
		}

		/* You stop referencing them and let the garbage collector take
		 * them. When you want to free the object, add the following
		 * line: obj = null; The the garbage collector if free to delete
		 * the object (provided there are no other pointer to the object
		 * that keeps it alive.) */
	}

	class Thread_queue_main {
		/* The Main method is the entry point of a C# application. When the
		 * application is started, the Main method is the first method that is
		 * invoked. */
		static void Main(string[] args) {
			/* Spefify a Van OS thread queue element. */
			VOS.Thread_queue_elem  elem;
			
			/* To create the queue element object, use the keyword new with
			 * the initialization arguments: */
			elem = new VOS.Thread_queue_elem(42);
			
			/* Print the current state of the Van OS thread queue element. */
			elem.Trace();
		}
	}
}
