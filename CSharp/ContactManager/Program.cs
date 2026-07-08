using System;
using System.Reflection.Metadata;

class Contact
{
    public string Name;
    public string PhoneNumber;
    public string Email;
}


class Program
{
    public static void Main()
    {
        List<Contact> contacts = new List<Contact>();
        while (true)
        {
            Console.WriteLine("1. Add contact");
            Console.WriteLine("2. Show all contacts");
            Console.WriteLine("3. Search contact");
            Console.WriteLine("4. Delete contact");
            Console.WriteLine("5. Exit");

            int input = int.Parse(Console.ReadLine());
            if (input == 1)
            {
                Console.WriteLine($"{input}.");
                Console.WriteLine("Name: ");
                string name = Console.ReadLine();

                Console.WriteLine("Phonenumber: ");
                string phonenumber = Console.ReadLine();

                Console.WriteLine("Email:  ");
                string email = Console.ReadLine();

                contacts.Add(new Contact
                {
                    Name = name,
                    PhoneNumber = phonenumber,
                    Email = email
                });

            }
        }
    }
}
