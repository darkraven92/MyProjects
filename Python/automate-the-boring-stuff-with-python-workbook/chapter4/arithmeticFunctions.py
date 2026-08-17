"""
def after_transaction(balance, transaction):
    return balance+transaction

balance=int(input("Enter balance: "))
transaction = int(input("Enter transaction: "))
print(after_transaction(balance,transaction))
"""

def plus_one(number):
    return number + 1

def add(number1, number2):
    total_sum=number1
    for i in range(number2):
        total_sum = plus_one(total_sum)
    return total_sum
number1 = int(input())
number2 = int(input())
print(add(number1,number2))
