print("Enter C or F to indicate Celsius or Fahrenheit:")
scale = input()
print("Enter the number of degrees:")
degrees = int(input())

if (scale == "C" and 16 <= degrees <= 38) or (
    scale == "F" and 60.8 <= degrees <= 100.4
):
    print("Safe")
else:
    print("Dangerous")