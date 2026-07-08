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
            Console.WriteLine("5. Update contact");
            Console.WriteLine("6. Exit");

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
            else if (input == 2)
            {
                Console.WriteLine("Show all contacts: ");
                foreach (Contact contact in contacts)
                {
                    Console.WriteLine($"Name: {contact.Name}");
                    Console.WriteLine($"Phonenumber: {contact.PhoneNumber}");
                    Console.WriteLine($"Email: {contact.Email}");

                }
            }
            else if (input == 3)
            {
                Console.WriteLine("Search: ");
                string searchName = Console.ReadLine();

                bool found = false;

                foreach (Contact contact in contacts)
                {
                    if (contact.Name == searchName)
                    {
                        Console.WriteLine($"Name: {contact.Name}");
                        Console.WriteLine($"Phonenumber: {contact.PhoneNumber}");
                        Console.WriteLine($"Email: {contact.Email}");

                        found = true;
                    }

                }
                if (found == false)
                {
                    Console.WriteLine("Contact not found");
                }
            }
            else if (input == 4)
            {
                Console.WriteLine("Delete contact: ");
                string deleteName = Console.ReadLine();
                Contact contactToDelete = null;

                foreach (Contact contact in contacts)
                {
                    if (contact.Name == deleteName)
                    {
                        contactToDelete = contact;
                        break;
                    }
                }
                if (contactToDelete != null)
                {
                    contacts.Remove(contactToDelete);
                    Console.WriteLine($"{contactToDelete.Name} deleted");
                }
                else
                {
                    Console.WriteLine("Contact not found");
                }
            }
            else if (input == 5)
            {
                string updateName = Console.ReadLine();
                Contact contactToUpdate = null;
                foreach (Contact contact in contacts)
                {
                    if (contact.Name == updateName)
                    {
                        contactToUpdate = contact;
                        break;
                    }
                }
                if (contactToUpdate != null)
                {
                    Console.WriteLine("New phone number:");
                    contactToUpdate.PhoneNumber = Console.ReadLine();

                    Console.WriteLine("New email:");
                    contactToUpdate.Email = Console.ReadLine();

                    Console.WriteLine("Contact updated");
                }
                else
                {
                    Console.WriteLine("Contact not found");
                }
            }
            else if (input == 6)
            {
                Console.WriteLine("Exit");
                break;
            }
        }
    }
}
