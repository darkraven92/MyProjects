import random
from pathlib import Path
def load_words():
    script_dir = Path(__file__).parent
    file_path = script_dir / "words.txt"

    try:
        with open(file_path,"r") as file:
            words = file.read().splitlines()
            return words
    except FileNotFoundError:
        print("Kunde inte hitta words.txt")
        return ["python", "code", "raider"]

def pcick_secret_word(words):
    return random.choice(words)


def main():
    words = load_words()
    secret_word = pcick_secret_word(words)
    guessed_letters = []
    

    print(secret_word)

    guess = input("Guess a letter: ")
    guessed_letters.append(guess)

    if guess in secret_word:
        for letter in secret_word:
            pass
            


main()

