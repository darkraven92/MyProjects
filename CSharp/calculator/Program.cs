using System;

public class CalculatorOperation
{
    private int _x;
    private int _y;

    public CalculatorOperation(int x, int y)
    {
        _x = x;
        _y = y;
    }
    public int Addition
    {
        get { return _x + _y; }
    }
    public int Subtraction
    {
        get { return _x - _y; }
    }
    public int Multiplication
    {
        get { return _x * _y; }
    }
    public double Division
    {
        get
        {
            if (_y == 0)
            {
                throw new DivideByZeroException();
            }

            return (double)_x / _y;
        }
    }

}
public class Program
{
    public static void Main()
    {
        while (true)
        {
            Console.WriteLine("1. Addition");
            Console.WriteLine("2. Subtraction");
            Console.WriteLine("3. Multiplication");
            Console.WriteLine("4. Division");
            Console.WriteLine("5. Exit");
            int input = int.Parse(Console.ReadLine());
            if (input == 1)
            {
                Console.WriteLine($"Choice: {input}");
                Console.WriteLine("First number: ");
                int x = int.Parse(Console.ReadLine());
                Console.WriteLine("Second number: ");
                int y = int.Parse(Console.ReadLine());
                CalculatorOperation operation = new CalculatorOperation(x, y);
                Console.WriteLine($"{x} + {y} = {operation.Addition}");
            }
            if (input == 2)
            {
                Console.WriteLine($"Choice: {input}");
                Console.WriteLine("First number: ");
                int x = int.Parse(Console.ReadLine());
                Console.WriteLine("Second number: ");
                int y = int.Parse(Console.ReadLine());
                CalculatorOperation operation = new CalculatorOperation(x, y);
                Console.WriteLine($"{x} - {y} = {operation.Subtraction}");
            }
            if (input == 3)
            {
                Console.WriteLine($"Choice: {input}");
                Console.WriteLine("First number: ");
                int x = int.Parse(Console.ReadLine());
                Console.WriteLine("Second number: ");
                int y = int.Parse(Console.ReadLine());
                CalculatorOperation operation = new CalculatorOperation(x, y);
                Console.WriteLine($"{x} * {y} = {operation.Multiplication}");

            }
            if (input == 4)
            {
                Console.WriteLine($"Choice: {input}");
                Console.WriteLine("First number: ");
                int x = int.Parse(Console.ReadLine());
                Console.WriteLine("Second number: ");
                int y = int.Parse(Console.ReadLine());
                CalculatorOperation operation = new CalculatorOperation(x, y);
                Console.WriteLine($"{x} / {y} = {operation.Division}");
            }
            if (input == 5)
            {
                Console.WriteLine($"Choice: {input}");
                Console.WriteLine("Bye");
                break;
            }
        }
    }
}
