using System;

class Calc{
	public int Add(int x, int y){
		return x + y;
	}
}

class Program{
	public static void Main(){
		Calc c = new Calc();
		int ans = c.Add(2,29);
		Console.WriteLine(ans);

	}
}

