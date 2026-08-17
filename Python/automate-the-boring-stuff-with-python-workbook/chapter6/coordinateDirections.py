def get_end_coordinates(directions):
    x = 0
    y = 0

    for direction in directions:
        d = direction.upper()
        if d == "N":
            y += 1
        elif d == "S":
            y -= 1
        elif d == "E":
            x += 1
        elif d == "W":
            x -= 1

    return [x, y]


# --- Huvudprogram ---
directions_list = []

while True:
    move = input("Enter direction (N, S, E, W or blank to stop): ")
    if move == "":
        break
    directions_list.append(move)

final_coordinates = get_end_coordinates(directions_list)
print(final_coordinates)