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
"""
def analysera_fil(filnamn):
    antal_rader = 0
    total_tecken = 0

    with open(filnamn, "r", encoding="utf-8") as f:
        for rad in f:
            antal_rader += 1
            rensad_rad = rad.strip("\n")
            total_tecken += len(rensad_rad)
    print(f"Antal rader: {antal_rader}")
    print(f"Antal tecken: {total_tecken}")

def main():
    filnamn = "workfile.txt"
    with open(filnamn, "w", encoding="utf-8") as f:
        f.write("Hej på dig!\nDet här är en testfil.")
    analysera_fil(filnamn)
    

main()
"""
"""
def analysera_logg(filnamn, sokord):
    antal_rader = 0
    traffar = 0

    with open(filnamn,"r",encoding="utf-8") as f:
        for rad in f:
            antal_rader +=1
            if sokord in rad:
                traffar += 1
        print(f"Totalt antal loggrader:{antal_rader}")
        print(f"Antal rader med {sokord}: {traffar}")


def main():

    loggdata = (
        "INFO: Systemet startade\n"
        "ERROR: Kunde inte ansluta till databasen\n"
        "INFO: Användare loggade in\n"
        "ERROR: Filen hittades inte\n"
        "WARNING: Lite minne kvar\n"
    )

    filnamn = "workfile2.txt"
    with open(filnamn,"w",encoding="utf-8") as f:
        f.write(loggdata)
    sokord = input()
    analysera_logg(filnamn, sokord)
main()
"""
"""
import json

def spara_anvandare(filnamn,data):
    with open(filnamn,"w",encoding="utf-8") as f:
        json.dump(data,f,indent=4)
def ladda_anvandare(filnamn):
    with open(filnamn,"r",encoding="utf-8") as f:
        return json.load(f)

def main():
    anvandare = {
    "alice": {"namn": "Alice", "alder": 28, "roller": ["admin", "utvecklare"]},
    "bob": {"namn": "Bob", "alder": 34, "roller": ["anvandare"]},
    }
    filnamn = "workfile3.json"
    spara_anvandare(filnamn,anvandare)
    hamtad_data = ladda_anvandare(filnamn)
    print("Inläst data: ")
    print(hamtad_data)
main()
"""