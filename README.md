# CHEESECAKE Prototype — Návod k obsluze a Checklist

Tento dokument slouží jako kompletní návod k obsluze robota, popisuje funkce tlačítek a obsahuje důležitý předstartovní checklist pro spolehlivý provoz na soutěži.

---

## 🕹️ Ovládání robota (Tlačítka na RBCX)

Po zapnutí robot čeká na výběr režimu (svítí **žlutá LED**). Režim se volí stiskem jednoho z tlačítek na desce RBCX:

*   **`UP` (Nahoru)**: **Celá jízda na medvěda** (Spustí `hledej_robota_konzervativni(true)` – provede cestu do hřiště a následně vyhledání a odvoz medvěda).
*   **`ON` (Zapnout)**: **Pouze vyhledávání medvěda** (Spustí `hledej_robota_konzervativni(false)` – přeskočí jízdu do hřiště a začne ihned vyhledávat medvěda na místě).
*   **`DOWN` (Dolů)**: **Otestování klepet** (Zavře klepeta na místě a zůstane v čekací smyčce).
*   **`LEFT` (Doleva)**: Spustí rutinu **`roadside_prava`** (sekvence pro značky na pravé straně).
*   **`RIGHT` (Doprava)**: Spustí rutinu **`roadside_leva`** (sekvence pro značky na levé straně).
*   **`OFF` (Vypnout)**: 
    *   *Před startem*: Spustí testovací sekvenci `roadside_test`.
    *   *Po dokončení jízdy (v hlavní smyčce)*: Vypíše kompletní uložené záznamy (**Log Buffer**) do sériové linky pro analýzu jízdy.

---

## 📋 Předstartovní checklist (Společný)

Před **každým** spuštěním zkontrolujte následující body:

1.  🔋 **Baterie**: Vždy používejte **plně nabitou** hlavní baterii. Slabá baterie způsobuje nepřesnosti v odometrii, slabší stisk klepet a může aktivovat pípání hlídače napětí.
2.  🧹 **Kola**: Očistěte obě hnací kola od prachu a nečistot. Snížíte tím prokluzování na dráze a zpřesníte jízdu.
3.  👐 **Klepeta**: Zkontrolujte, zda se klepeta pohybují hladce a serva nejsou mechanicky blokována.
4.  📦 **Uchycení baterie**: Ujistěte se, že baterie pevně drží v držáku a **netahá se po zemi**.
5.  🔘 **Zadní tlačítka**: Otestujte zadní spínače pro detekci nárazu do stěny – musí volně pružit a spolehlivě spínat.
6.  📡 **IR Senzory**: Zkontrolujte přední IR senzory. Ty slouží k detekci překážky přímo před robotem (reagují na to, že před ně něco položíte/hodíte).
7.  🎯 **Nulování gyroskopu**: 
    *   Tlačítko **RESET** na RBCX stiskněte **pouze poté, co vložíte robota na startovní pozici**.
    *   Při startu a kalibraci držte robota **naprosto v klidu**, aby se gyroskop správně zkalibroval na nulový úhel.

---

## 🛣️ Instrukce pro sekvenci Roadside (Značky)

*   🏋️ **Závaží**: Pro rutinu Roadside je **nutné přidat na robota závaží**, aby měla kola dostatečnou trakci při manipulaci s těžkou značkou.
*   📐 **Startovní pozice**:
    *   **Osa X**: Střed domovského startovního hřiště.
    *   **Osa Y**: Klepeta umístěte **co nejblíže k robotovi (co nejvíce otevřená/zatažená)** tak, aby byl zadek robota co nejdále od startovní stěny.
*   ⏱️ **Trik pro přeskočení značek (Simulovaný restart)**:
    *   Rutina Roadside obsahuje hlídání překážek pomocí ultrazvukového senzoru.
    *   Pokud nechcete, aby robot po otočení pokračoval na samotnou část se značkami, **stoupněte si těsně před ultrazvukový senzor** (do vzdálenosti **pod 40 cm / 340 mm**).
    *   Robot detekuje překážku, rozsvítí žlutou LED a po 2 měřeních provede `ESP.restart()`, čímž se bezpečně zastaví a vrátí do úvodního menu.

---

## 🐻 Instrukce pro Bear Rescue (Vyhledávání medvěda)

Pro správnou spolupráci s Raspberry Pi (zpracování obrazu z kamery):

1.  🔌 **Powerbanka**: Plně nabijte powerbanku pro Raspberry Pi a propojte ji napájecím kabelem s Raspberry Pi.
2.  📹 **Kamera a kabel**: 
    *   Zkontrolujte správné nasměrování kamery (**musí mířit přímo dopředu robota**).
    *   Zkontrolujte zapojení propojovacího kabelu kamery. **Vždy mějte s sebou náhradní kabel!**
3.  🚀 **Postup spuštění programu na Raspberry Pi**:
    *   Před spuštěním programu na Pi **zapněte hlavní spínač RBCX** (aby běžela komunikace).
    *   Na Raspberry Pi přejděte na **Desktop** (Plochu) – skript se spouští pouze odtud.
    *   Poklepejte na soubor `main.py` -> otevře se vývojové prostředí **Thonny**.
    *   V horní liště Thonny klikněte na **červené/zelené tlačítko pro spuštění (Run / Start)**.
4.  🛑 **Ukončení jízdy**:
    *   Jakmile robot úspěšně dojede do cílového pole s medvědem, **ihned stiskněte tlačítko RESET** na RBCX.
    *   **Robot se sám na konci nezastaví** a zůstane stát se spuštěnými motory / v nekonečné smyčce.
