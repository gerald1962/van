/* The using System line means that you are using the System library in your
 * project. Which gives you some useful classes and functions like Console class
 * or the WriteLine function/method. */
using System;

/* Include the Dictionary, to extend a Van OS message with generic fields. */
using System.Collections.Generic;

/* The Thread class is defined in the System. Threading namespace that must be
 * imported before you can use any threading related types, like Mutex. */
using System.Threading;

/* The namespace VOS - Van OS - is used to organize its code, and it is a
 * container for VOS classes. Namespace also solves the problem of naming
 * conflict. */
namespace VanOS {
	/* Structs are similar to classes in that they represent data structures
	 * that can contain data members and function members. However, unlike
	 * classes, structs are value types and do not require heap
	 * allocation. */
	public struct Sys {
		/* Van OS max. number of the queue elements. */
		public const int  Tq_limit = 1024;

		/* Use the static modifier to declare a static member, which
		 * belongs to the type itself rather than to a specific
		 * object. */
		
		/* If the condition being tested is not met, an exception is
		 * thrown. */
		public static void Assert(bool cond) {
			/* Test the exception condition. */
			if (cond)
				return;

			/* Raised when a method call is invalid in an object's
			 * current state. */
			throw new InvalidOperationException("Invalid operation");
		}
	}
	
	/* Everything in C# VOS is associated with thread or thread input queue
	 * classes and objects, along with its attributes and methods. */
	
	/* Tm_msg is the message class of a Van OS thread input queue with
	 * i class members: j fields and k methods. */
	public class Tm_msg  {
		/* A delegate is an object which refers to a method or you can
		 * say it is a reference type variable that can hold a reference
		 * to the methods. Delegates in C# are similar to the function
		 * pointer in C/C++. It provides a way which tells which method
		 * is to be called when an event is triggered. */
                public delegate void Tm_cb(Tm_msg msg);
		
		/* Successor of the thread input queue. */
		public Tm_msg  next;

		/* Callback or reference to a function to process the input
		 * message. It is expected to execute this field at a given time
		 * in the thread context. */
		public Tm_cb  cb;

		/* Dictionary is a generic collection which is generally
		 * used to store key/value pairs: declare a Dictionary
		 * containing just the type object and then cast your
		 * results. */
		public Dictionary<string, object>  param;
		
		/* You stop referencing them and let the garbage collector take
		 * them. When you want to free the object, add the following
		 * line: obj = null; The the garbage collector if free to delete
		 * the object (provided there are no other pointer to the object
		 * that keeps it alive.) */

		/* The constructor Tm_msg() is a special method that is used
		 * to initialize the Tm_msg object implicitely associated with
		 * the new VanOS.Tm_msg(() method. */
		public Tm_msg(Tm_cb msg_cb) {
			/* Save the  reference to a function to process the input
			 * message in the thread context. */
			cb = msg_cb;

			/* Create the Dictionary object, to extend the input
			 * message with any key value pair optinally: e.g.
			 * msg.param.Add("name",  "msg_1");
			 * msg.param.Add("count", 0); ... */
			param = new Dictionary<string, object>();
		}
	}

	/* Tq_queue is the input queue class of a Van OS thread with i class
	 * members: j fields and k methods. */
	public class Tq_queue {
		/* Synchronize the access to the protected message queue. */
		public Mutex  mutex;

		/* First empty queue element. */		
		private Tm_msg  anchor;
		
		/* Last empty queue element. */
		private Tm_msg  stopper;
		
		/* Current max. number of the queue elements. */
		private int  limit;

		/* Current number of the input queue elements. */
		public int  count;

		/* If 1, a client executes Tq_send(). */
		public int  busy_send;

		/* Calculate the next message from the thread input queue. */
		public Tm_msg Tq_receive() {
			Tm_msg  msg;
			
			/* Wait until it is safe to enter the critical section. */
			this.mutex.WaitOne();
			
			/* Test the queue state. */
			if (this.count < 1) {
				/* Release the mutex to leave the critical section. */
				this.mutex.ReleaseMutex();

				/* The thread input queue is empty. */
				return null;
			}
			
			/* Update the number of the queue elements. */
			this.count--;
			
			/* Get the first queue element. */
			msg = this.anchor.next;

			/* Calculate the new queue start. */
			this.anchor.next = msg.next;

			/* Test the outside located limits of the input
			 * queue. */
			if (this.count < 1) {
				/* Test the queue state. */
				Sys.Assert(this.anchor.next == this.stopper);

				/* As of now, the input queue is empty. */
				this.stopper.next = this.anchor;
			}

			/* Release the mutex to leave the critical section. */
			this.mutex.ReleaseMutex();

			/* Return the reference to current input message. */
			return msg;
		}
		
		/* Extend the input queue of a Van OS thread and resume it. */
		public void Tq_send(Tu_thread t, Tm_msg msg) {
			int is_running;

			/* Entry condition. */
			Sys.Assert(! (t == null || msg == null || msg.cb == null ||
				      t.exec_s != Tu_thread.Exec_s.VOS_THD_READY));
			
			/* Change the state of this operation. */
			this.busy_send = 1;

			/* Wait until it is safe to enter the critical section
			 * of the input queue. */
			this.mutex.WaitOne();

			/* Update the number of the queue elements. */
			this.count++;

			/* Test the fill level of the message queue. */
			Sys.Assert(this.count <= this.limit);

			/* Insert the new message at the end of the queue. */
			this.stopper.next.next = msg;
			msg.next = this.stopper;
			this.stopper.next = msg;

			/* Change the execution state of the thread. */
			is_running = t.is_running;
			t.is_running = 1;
			
			/* Release the mutex to leave the critical section. */
			this.mutex.ReleaseMutex();

			/* Test the execution state of the thread. */
			if (is_running == 0) {
				/* Resume this thread blocked in the thread
				 * control semaphore. */
				t.suspend_c.Release();
			}
			
			/* Change the state of this operation. */
			this.busy_send = 0;
		}

		/* Free the queue ressources. */
		public void Tq_destroy() {
			/* Free the first queue message. */
			this.anchor.param = null;
			this.anchor = null;
			
			/* Free the last queue message. */
			this.stopper.param = null;
			this.stopper = null;
			
			this.mutex = null;
		}
		
		/* The constructor Tq_queue() is a special method that is used
		 * to initialize the Tq_queue object implicitely associated with
		 * the new VanOS.Tq_queue() method. */
	        public Tq_queue(int queue_limit) {
			/* Entry condition. */
			Sys.Assert(queue_limit < Sys.Tq_limit);
			
			/* Save the maximum number of the queue elements. */
			this.limit = queue_limit + 2;

			/* Create the mutex, to synchronize the access to the
			 * protected message queue. */
			this.mutex = new Mutex();
			
			/* To create the queue element object, use the keyword new with
			 * the initialization arguments: */

			/* Define the excluded limits of a Van OS thread input
			 * queue like ] ... [ or first and last element of the
			 * Van OS thread input queue. Note: these messages shall
			 * never be consumed, therefore the new argument
			 * callback is null. */
			this.anchor  = new VanOS.Tm_msg(null);
			this.stopper = new VanOS.Tm_msg(null);

			/* Link the first and last input queue element. */
			this.anchor.next  = this.stopper;
			this.stopper.next = this.anchor;
			
			/* Initialize the boundary conditions of a thread input
			 * queue. */
			this.count = 0;
		}
	}

	/* Tu_thread is the Van OS user thread class with i class members:
	 * j fields and k methods. */
	public class Tu_thread {
		/* The public keyword is called an access modifier, which
		 * specifies that the fields are accessible for other classes. */

		/* Execution states of a Van OS thread. */
		public enum Exec_s { VOS_THD_BOOT, VOS_THD_READY, VOS_THD_KILL,
			VOS_THD_INV };

		/* Initial execution state of a Van OS thread. */
		public Exec_s  exec_s;
		
		/* Synchronize the access to the multi thread access to the
		 * thread state. */
		private Mutex  mutex;

		/* Name of a Van OS thread. */
		public string  name;

		/* Reference to a C sharp thread object. 
		 * Once the task assigned to a Thread is completed, that thread
		 * will be terminated and we don’t need to worry about it. */
		private Thread  thread; 
		
		/* Input queue of a Van OS thread. */
		private Tq_queue  queue;

		/* A semaphore, that shall suspend this child thread, installed
		 * by the superordinate or parent thread. */
		public Semaphore  suspend_c;

		/* The parent shall be blocked until the child thread has been
		 * started. */
		private Semaphore  suspend_p;

		/* 1, if this thread is running on any CPU. */
		public int is_running;
		
		/* Extend the input queue of a Van OS thread and resume it. */
		public void Tu_send(Tm_msg msg) {
			/* Extend the input queue of a Van OS thread and resume it. */
			this.queue.Tq_send(this, msg);
		}

		/* Private members are accessible only within the body of the
		 * class or the struct in which they are declared. */

		/* Process all current messages. */
		private void Tu_receive() {
			Tm_msg  msg;
			
			/* Process the received messages. */
			for (;;) {
				/* Get the next thread input message. */
				msg = this.queue.Tq_receive();
				
				/* Test the message state. */
				if (msg == null)
					break;
				
				/* Execute the message actions. */
				Sys.Assert(msg.cb != null);
				msg.cb(msg);
				
				/* Free the consumed message. */
				msg.param = null;
				msg = null;
			}
		}
			
		
		/* Suspend the thread, if the message queue is empty or it is
		 * alive. */
		private bool Tu_suspend() {
			Tq_queue  q;
			int  count;

			/* Get the reference to the thread input queue. */
			q = this.queue;
			
			/* Wait until it is safe to enter the critical section
			 * of the input queue. */
			q.mutex.WaitOne();
						
			/* If the input queue is empty, prepare the suspend
			 * operation for this thread. */
			this.is_running = 0;

			/* Copy the filling level of the input queue. */
			count = q.count;

			/* Leave the critical section. */
			q.mutex.ReleaseMutex();

			/* If the input queue is empty, suspend the thread or
			 * terminate the thread. */
			if (count < 1) {
				/* Test the shutdown request for this thread. */
				if (this.exec_s == Exec_s.VOS_THD_KILL)
					return false;

				/* Suspend this thread, until it is resumed by a
				 * input message or shutdown trigger. */
				this.suspend_c.WaitOne();
			}

			/* Precondition: either at least there is a pending
			 * input message or this thread shall be killed. */

			/* This thread is running on any CPU. */
			this.is_running = 1;
			
			/* Test the shutdown request for this thread. */
			if (this.exec_s == Exec_s.VOS_THD_KILL)
				return false;

			/* Final condition: there are pending input messages
			 * and this thread is alive. */
			return true;
		}
		
		/* A new thread shall execute the callback method Tm_cb(). */
		private void Tu_cb() {
			/* Update the Van OS thread state. */
			exec_s = Exec_s.VOS_THD_READY;

			/* Resume the parent thread in the constructor
			 * Tu_thread(), which has created and started this child
			 * thread. */			
			this.suspend_p.Release();

			/* Process all received messages or accept the shutdown
			 * request for this thread. */
			for (;;) {
				/* Suspend the thread, if the message queue is
				 * empty or the thread is alive. */
				if (! Tu_suspend()) {
					/* This thread shall be killed. */
					break;
				}

				/* Process all current messages. */
				Tu_receive();
			}

			/* This child thread shall be removed. */

			/* Resume the parent thread in Tu_destroy(). */
			this.suspend_p.Release();
		}

		/* Shutdown a Van OS user thread. */
		public void Tu_destroy() {
			/* Get, modify and test the thread state. */
			this.mutex.WaitOne();

			/* Entry condition. */
			Sys.Assert(this.exec_s == Exec_s.VOS_THD_READY &&
				   this.queue.busy_send != 1);

			/* Change the thread state. */
			this.exec_s = Exec_s.VOS_THD_KILL;

			/* Leave the critical section. */
			this.mutex.ReleaseMutex();

			/* Resume this Van OS thread in Tu_suspend(). */
			this.suspend_c.Release();

			/* The parent thread waits for the termination of the
			 * child thread in Tu_cb(). */
			this.suspend_p.WaitOne();

			/* Free the input queue ressources. */
			this.queue.Tq_destroy();

			/* Free the thread ressources. */
			this.queue     = null;
			this.thread    = null;
			this.suspend_p = null;
			this.suspend_c = null;
			this.mutex     = null;
		}
		
		/* A constructor is a special method that is used to initialize
		 * objects. The advantage of a constructor, is that it is called
		 * when an object of a class is created. It can be used to set
		 * initial values for fields: */
		public Tu_thread(string thd_name, int queue_limit) {
			/* Initialize the execution state of a Van OS user
			 * thread. */
			exec_s = Exec_s.VOS_THD_BOOT;

			/* Copy the thread name. */
			name = thd_name;

			/* Create the mutex to synchronize the access to the
			 * multi thread access to the thread state. */
			mutex = new Mutex();

			/* Create a semaphore that can satisfy up to 1
			 * concurrent request. Use an initial count of zero, so
			 * that the entire semaphore count is initially owned by
			 * this child thread. */
			suspend_c = new Semaphore(0, 1);

			/* Create a semaphore to control the parent thread. */
			suspend_p = new Semaphore(0, 1);
			
			/* Allocate a C sharp thread object. */
			thread = new Thread(new ThreadStart(Tu_cb));
			
			/* Allocate the input queue for this thread. */
			queue = new Tq_queue(queue_limit);

			/* Start the user thread, which shall execute the callback
			 * method Tu_cb(). */
			thread.Start();

			/* The parent thread shall be blocked until this child
			 * thread has been started. */
			suspend_p.WaitOne();

			/* Exit condition. */
			Sys.Assert(this.exec_s == Exec_s.VOS_THD_READY);
		}
	}
}
