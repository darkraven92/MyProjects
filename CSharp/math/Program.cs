using System;
namespace Math
{
    class Triangle
    {
        public int Bas;
        public int Height;

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
            Triangle triangle = new Triangle(2, 3);
            int area = (triangle.Bas * triangle.Height) / 2;
            Console.WriteLine(area);
        }
    }
}
