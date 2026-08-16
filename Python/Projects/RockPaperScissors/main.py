import random
def computer(choice):
    computerchoice = random.choice(choice)
    return computerchoice

def player(choice):
    while True:
        playerchoice = input("Move: ").lower()
        if playerchoice not in choice:
            print("Invalid choice")
        else:
            return playerchoice

def winingcondition(playerchoice, computerchoice):
    if playerchoice == computerchoice:
        print("Draw!")
        return "draw"
    # Samlar alla tre fall där spelaren vinner
    elif (
        (playerchoice == "rock" and computerchoice == "scissors")
        or (playerchoice == "paper" and computerchoice == "rock")
        or (playerchoice == "scissors" and computerchoice == "paper")
    ):
        print("Player wins this round!")
        return "player"
    # Om det inte blev oavgjort och spelaren inte vann, vann datorn
    else:
        print("Computer wins this round!")
        return "computer"
    
def main():
    computerwins = 0
    playerwins = 0
    max_wins = 3
    choice = ["rock","paper","scissors"]

    while computerwins < max_wins and playerwins < max_wins:
        computerchoice = computer(choice)
        playerchoice = player(choice)
        print(f"Player: {playerchoice}")
        print(f"Computer: {computerchoice}")
        winner = winingcondition(playerchoice,computerchoice)
        if winner == "player":
            playerwins += 1
        elif winner == "computer":
            computerwins += 1

    print("\n---------------------------------------")
    if playerwins == max_wins:
        print("Congratulations! You won the entire game!")
    else:
        print("Game Over! Computer won the game!")
    print("-----------------------------------------")

main()


