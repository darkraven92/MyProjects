using System;
using Math;
namespace Math
{
    class Triangle
    {
        private int _bas;
        private int _heigth;

        public int Bas
        {
            get { return _bas; }
            set
            {
                if (value <= 0)
                {
                    throw new ArgumentException("Base have to be greater than 0");
                }
                _bas = value;
            }
        }
        public int Height
        {
            get { return _heigth; }
            set
            {
                if (value <= 0)
                {
                    throw new ArgumentException("Height have to be greater than 0");
                }
                _heigth = value;
            }
            public double Area
        {
            get
            {
                return (_bas * _heigth) / 2.0;
            }
        }
        }

        //Konstruktor
        public Triangle(int bas, int height)
        {
            Bas = bas;
            Height = height;
        }
    }
    class Program
{
    static void Main(string[] args)
    {
        Console.WriteLine("Base: ");
        int bas = int.Parse(Console.ReadLine());
        Console.WriteLine("Height: ");
        int height = int.Parse(Console.ReadLine());

        try
        {
            Triangle triangle = new Triangle(bas, height);
            Console.WriteLine($"Area: {triangle.Area} ");
        }
        catch (ArgumentException ex)
        {
            Console.WriteLine($"Error: {ex.Message}");
        }
    }
}
