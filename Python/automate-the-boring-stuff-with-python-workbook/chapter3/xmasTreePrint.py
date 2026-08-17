
import random

"""

size = int(input("Enter the tree size: "))

for row_num in range(1, size + 1):
    spaces = " " * (size - row_num)

    # Bygg grenraden bokstav för bokstav
    branches = ""
    num_chars = row_num * 2 - 1
    for _ in range(num_chars):
        if random.randint(1, 4) == 1:
            branches += "o"
        else:
            branches += "^"

    print(spaces + branches)

# Stam
for _ in range(2):
    print(" " * (size - 1) + "#")

"""

import random

size = int(input("Enter the tree size: "))

row_num = 1
while row_num <= size:
    spaces = " " * (size - row_num)

    branches = ""
    num_chars = row_num * 2 - 1
    char_count = 0

    while char_count < num_chars:
        if random.randint(1, 4) == 1:
            branches += "o"
        else:
            branches += "^"
        char_count += 1

    print(spaces + branches)
    row_num += 1

trunk_count = 0
while trunk_count < 2:
    print(" " * (size - 1) + "#")
    trunk_count += 1