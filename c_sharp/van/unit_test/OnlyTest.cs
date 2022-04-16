using System;
using System.Text;
using System.Collections.Generic;
using NUnit.Framework;

[TestFixture]
public class OnlyTest
{
    [Test]
    public void MyTest() 
    {
        int a = 10;
        Assert.AreEqual(10, a);
    }
}
