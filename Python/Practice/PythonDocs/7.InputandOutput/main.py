"""
def skriv_ut_kvitto(produkter):

    totalpris = 0
    for namn, pris in produkter:
        print(f"{namn:15} {pris:10.2f}kr")
        totalpris += pris

    print("-" * 30)
    print(f"{'Totalt':15} {totalpris:10.2f} kr")
        

def main():
    produkter = [("Kaffe", 35.5), ("Kanelbulle", 28.0), ("Apelsinjuice", 42.9)]
    skriv_ut_kvitto(produkter)
main()
"""
"""
def skriv_ut_resultat(elever):
    antalpoang = 0
    antalelever = 0
    for namn, poang in elever:
        print(f"{namn:12} {poang:8.1f}")
        antalpoang += poang
        antalelever += 1

    print("-" * 30)
    print(f"{'Medelpoäng':12} {antalpoang/antalelever:8.1f}")

def main():
    elever = [("Alice", 85.5), ("Bob", 92.0), ("Charlie", 78.25), ("Diana", 88.0)]
    skriv_ut_resultat(elever)
main()  
"""

def analysera_fil(f):
    with open("workfile.txt") as f:
        print(f.read())
    

def main():
    with open("workfile.txt", "w", encoding="utf-8") as f:
        f.write("Hej på dig!\nDet här är en testfil.")
    analysera_fil(f)
    

main()
