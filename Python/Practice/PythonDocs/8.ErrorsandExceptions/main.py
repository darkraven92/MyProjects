"""
def read_number(age):
    while True:
        try:
            prompt = input(age)
            return int(prompt)
        except ValueError:
            print("Invalid number")



def main():
    validAge = read_number("Enter your age: ")
    print(f"{validAge}")
    
main()
"""
"""
def dela_tal():
    while True:
        try:
            x = float(input("Enter x value: "))
            y = float(input("Enter y value: "))
            result = x /y
        except ValueError:
            print("Invalid numbers")
        except ZeroDivisionError:
            print("Division by 0")
        else:
            return result
        finally:
            print("--- Försök avklarat ---")

def main():
    result = dela_tal()
    print(f"Resultatet blir: {result}")
main()
"""
"""
def validera_losenord(losenord):
    if len(losenord) < 8:
        raise ValueError("Not enough characthers")
    return True

def main():
    losenord = input("Enter a password: ")
    try:
        validera_losenord(losenord)
    except ValueError as e:
        print(f"Fel: {e}")
    else:
        print("Good Password")
main()
"""
