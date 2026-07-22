#include<stdio.h>
#include<stdlib.h>
#include<time.h>

int main(){
    int number, randomNum, attempts = 0;
    srand(time(NULL));
    randomNum = rand() % 100 + 1;
    while(number != randomNum){
        printf("Guess a number: "); 
        scanf("%d",&number);
        attempts++;
        if(number<randomNum){
            printf("You guessed to low\n");
        }
        else if(number>randomNum){
            printf("You guessed to high\n");
        }
        else if ((number==randomNum)){
            printf("Correct!\n");
            printf("Amount of guesses: %d",attempts);

        }
        
    }
    return 0;

}