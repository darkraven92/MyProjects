def is_pangram(sentence):
    EACH_LETTER = []

    for char in sentence:
        # Omvandla till stor bokstav
        char_upper = char.upper()

        # Kolla om tecknet är en bokstav (A-Z) och inte redan finns i listan
        if char_upper.isalpha() and char_upper not in EACH_LETTER:
            EACH_LETTER.append(char_upper)

    # Returnera True om vi hittade alla 26 bokstäver, annars False
    return len(EACH_LETTER) == 26


# Huvudprogram
user_input = input("Enter a sentence:")

if is_pangram(user_input):
    print("That sentence is a pangram.")
else:
    print("That sentence is not a pangram.")