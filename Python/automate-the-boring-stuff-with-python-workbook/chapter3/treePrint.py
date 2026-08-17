"""
size = int(input("Enter the tree size: "))

# Grenar
for row_num in range(1, size + 1):
    spaces = " " * (size - row_num)
    branches = "^" * (row_num * 2 - 1)
    print(spaces + branches)

# Stam
for _ in range(2):
    print(" " * (size - 1) + "#")
"""

size = int(input("Enter the tree size: "))

row_num = 1
while row_num <= size:
    spaces = " " * (size - row_num)
    branches = "^" * (row_num * 2 - 1)
    print(spaces + branches)
    row_num += 1

trunk_count = 0
while trunk_count < 2:
    print(" " * (size - 1) + "#")
    trunk_count += 1