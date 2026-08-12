import random

def generate_number():
    return random.randint(1,100)

def main():
    attempts = 0
    randomnumber = generate_number()
    max_attempts = 10
    while attempts < 10:
        try:
            guess = int(input("Guess a number: "))
            attempts += 1
            
            if guess == randomnumber:
                print(f"You guessed the correct number: {guess}")
                print(f"Amount of guesses: {attempts}")
                break
            elif guess < randomnumber:
                print(f"Number larger than {guess}")
                
            elif guess > randomnumber:
                print(f"The number is smaller than {guess}")
           
            print(f"Amount of attempts: {attempts}")
        
        except ValueError:
            print("Enter a number")

main()