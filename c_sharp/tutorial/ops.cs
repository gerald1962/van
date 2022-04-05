using System;

/* Operators are used to perform operations on variables and values. */
namespace Ops {
	class Prg {
		static void Main(string[] args) {
			int  x, y;
			
			/* Lst of all assignment operators: */
			x = 1;
			y = 1;
			Console.WriteLine("Ops:  =     x  = 1 -> {0}    y = 1 -> {1}", x, y);

			x += 1;
			y  = y + 1;
			Console.WriteLine("Ops:  +=    x += 1 -> {0}    y = y + 1 -> {1}", x, y);

			x -= 1;
			y  = y - 1;
			Console.WriteLine("Ops:  +=    x -= 1 -> {0}    y = y - 1 -> {1}", x, y);
			
			/*					
*=	x *= 3	x = x * 3	
/=	x /= 3	x = x / 3	
%=	x %= 3	x = x % 3	
&=	x &= 3	x = x & 3	
|=	x |= 3	x = x | 3	
^=	x ^= 3	x = x ^ 3	
>>=	x >>= 3	x = x >> 3	
<<=	x <<= 3	x = x << 3	
			*/
		}
	}
}
