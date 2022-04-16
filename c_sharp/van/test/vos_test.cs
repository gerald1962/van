/* The using System line means that you are using the System library in your
 * project. Which gives you some useful classes and functions like Console class
 * or the WriteLine function/method. */
using System;

/* Include the Dictionary, to extend a Van OS message with generic fields. */
using System.Collections.Generic;

/* The Thread class is defined in the System. Threading namespace that must be
 * imported before you can use any threading related types, like Semaphore. */
using System.Threading;

/* Include Van OS classes like Tm_msg. */
using VanOS;

/* The namespace VOS - Van OS - is used to organize its code, and it is a
 * container for VOS classes. Namespace also solves the problem of naming
 * conflict. */
namespace Test {
	/* Test the thread concept. */
	class Prg {
		/* A semaphore, that shall suspend the Main() thread. */
		private static Semaphore  suspend;

		/* Size of the thread input queue. */
		private const int q_limit = 4;

		/* Max. number of the generated test messages. */
		private const int m_limit = 4;

		/* Current number of the generated test messages. */
		private static int count;
		
                /* Define the method to process a thread input message. */
                private static void msg_cb(Tm_msg msg) {
			Dictionary<string, object>  param;
			Tu_thread  t;
			string  s;
			int  i;

			/* Get the pointer to the Dictionary param. */
			param = msg.param;
			
			/* Using ContainsKey() method to check the specified key
			 * is present or not. */
			Sys.Assert(param.ContainsKey("thread") &&
				   param.ContainsKey("name") &&
				   param.ContainsKey("count"));
			
			/* Use the Dictionary key as index to get the value, but
			 * then you need to cast the results. */
			t = (Tu_thread) msg.param["thread"];
			s = (string)    msg.param["name"];
			i = (int)       msg.param["count"];

			Console.WriteLine("{0}, msg_cb: name = {1}, count = {2}",
					  t.name, s, i);

			/* Update the message counter. */
			count++;
			
			/* Test the queue exit condition of the child thread. */
			if (count < m_limit)
				return;

			/* Resume the main thread. */
			suspend.Release();
                }

		/* The Main method is the entry point of a C# application. When the
		 * application is started, the Main method is the first method that is
		 * invoked. */
		static void Main() {
			/* Spefify a Van OS user thread. */
			Tu_thread  thd_x;

			/* Current message counter. */
			int  i;
			
			/* Spefify a Van OS thread input message. */
			Tm_msg  msg;

			/* Message name. */
			string  n;
			
			/* Create the thread control semaphore. */
			suspend = new Semaphore(0, 1);

			/* Initialize the message counter. */
			count = 0;
			
			/* To create a Van OS idrt thread object, specify the class
			 * name, followed by the object name, and use the keyword new: */
			thd_x = new Tu_thread("thd_x", q_limit);
			
			/* Produce some messages for the thread input queue. */
			for (i = 0; i < m_limit; i++) {
				/* Create the input message for the Van OS
				 * thread with the message processing method
				 * msg_cb(), see above. */
				msg = new Tm_msg(msg_cb);
				
				/* String.Format performs the same operation a C
				 * snprintf(), but do note that the format
				 * strings are in a different format. */
				n = string.Format("msg-{0}", i);

				/* Extend the Van OS input message with generic
				 * parameters: add further message fields or
				 * key/value pairs to the message Dictionary
				 * using the Dictionary Add() method. */
				msg.param.Add("thread", thd_x);
				msg.param.Add("name",   n);
				msg.param.Add("count",  i);
				
				/* Extend the input queue of a Van OS user
				 * thread and resume it. */
				thd_x.Tu_send(msg);
			}

			/* Suspend the main thread, until the test thread has
			 * done its job. */
			suspend.WaitOne();

			/* Shutdown the test thread. */
			thd_x.Tu_destroy();

			/* Free the main() control semaphore. */
			suspend = null;
		}		
	}
}
