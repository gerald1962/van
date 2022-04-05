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
        unsafe class Program {
		// point to every functions that it has void as return value and with no input parameter
		public delegate void os_tq_cb(os_tq_elem *msg);
		
		public struct os_tq_elem {
			public os_tq_elem  *next;
			public void        *param;
			public os_tq_cb     cb;
		};
		
		/* Use DllImport to import the van lib. */
		[DllImport("../../../lib/libvan.so")]
		private static extern void os_init(int mask);
		
		[DllImport("../../../lib/libvan.so")]		
		private static extern void os_exit();
		
		[DllImport("../../../lib/libvan.so")]
		private static extern void os_trace_button(int n);
		
		[DllImport("../../../lib/libvan.so")]
		private static extern void os_mcs_hello();

		[DllImport("../../../lib/libvan.so")]
		private static extern void *os_thread_create(string name, int prio, int queue_size);

		[DllImport("../../../lib/libvan.so")]
		private static extern void os_thread_destroy(void *thread);

		[DllImport("../../../lib/libvan.so")]
		private static extern void os_queue_send(void *g_thread, os_tq_elem *msg, int size);

		public static void hello_cb(os_tq_elem *msg) {
			Console.WriteLine("hello_cb: ...");
		}
		
		/* Another thing that always appear in a C# program, is the Main
		 * method. */
		static void Main(string[] args) {
			os_tq_elem  msg;
			void  *tid;
			
			/* Initialize the van OS. */
			os_init(1);
			 
			/* Activate the OS trace. */
			os_trace_button(1);
			 
			/* Van OS welcomes C#. */
			
			os_mcs_hello();

			/* Install a test thread. */
			tid = os_thread_create("hello", 40, 2);

			msg.next  = null;
			msg.param = tid;
			msg.cb    = hello_cb;

			Console.WriteLine("hello_cb = {0}, sizeof(os_tq_elem) = {1}", msg.cb, sizeof(os_tq_elem));
			
			os_queue_send(tid, &msg, sizeof(os_tq_elem));

			/* XXX */
			System.Threading.Thread.Sleep(5000);
			
			/* Remove the test thread. */
			os_thread_destroy(tid);

			/* Free the van OS resources. */
			os_exit();
		 }
	}
}
