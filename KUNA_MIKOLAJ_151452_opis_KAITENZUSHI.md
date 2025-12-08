**Temat:** Temat 1 - Restauracja "Kaiten Zushi"

**Mikołaj Kuna GP02**

**Link do repozytorium na GitHub**
https://github.com/mkuna2004-byte/Restauracja-kaiten-zushi.git

Projekt polega na implementacji symulacji działania restauracji Kaiten Zushi z wykorzystaniem programowania współbieżnego opartego na procesach.
Głównym celem jest weryfikacja poprawności synchronizacji i komunikacji między aktorami symulacji (Klient, Kucharz, Obsługa, Kierownik) w dynamicznie zmieniających się warunkach (np. zmiana tempa produkcji, alokacja miejsc, klienci specjalni).


**Testy**

Projekt zostanie przetestowany w następujących scenariuszach:

**Test1: Test reakcji kucharza na sygnały**
Sprawdzenie, czy Kucharz poprawnie i natychmiast reaguje na dwa kolejne, przeciwstawne sygnały Kierownika (przyspieszenie, a następnie spowolnienie produkcji), wpływając na tempo zapełniania taśmy.

Przebieg: Natychmiastowa zmiana parametru czasowego Kucharza (sygnał 1 - dwa razy szybciej, sygnał 2 - o 50% wolniej) podczas trwającej symulacji.

**Test2: Test logiki rozmieszczenia klientów**
Weryfikacja, czy algorytm przydzielania stolików poprawnie uniemożliwia zajmowanie niepasujących miejsc i czy duża grupa oczekująca na jedyny duży stolik nie czeka zbyt długo przez ciągłe wpuszczanie małych, niepasujących grup.

Przebieg: Konfrontacja dużej grupy (np. 4-osobowej) z kilkoma małymi wolnymi zasobami (miejsca 2-osobowe).

**Test3: Test zamówień specjalnych**
Weryfikacja, czy talerzyk przeznaczony dla konkretnego klienta przy końcu taśmy nie zostanie pobrany przez klienta siedzącego bliżej kuchni, co sprawdza poprawne użycie flag docelowych.

Przebieg: Klient z dalekiego stolika składa zamówienie, a talerzyk musi minąć wszystkie poprzednie stoliki i dotrzeć do właściwego klienta.

**Test4: Test kasy**
Weryfikacja, czy proces płatności jest odpowiednio chroniony przed warunkami wyścigu, gdy wiele grup jednocześnie zgłasza chęć zapłaty.

Przebieg: Cztery różne grupy jednocześnie żądają rachunku, co weryfikuje sekwencyjną obsługę i poprawne naliczenie sumy w kasie.

**Test5: Test złożonej logiki specjalnych klientów**
Sprawdzenie logiki jednoczesnego obsługiwania klientów VIP i dzieci.

Przebieg: Jednoczesna obsługa grupy VIP oraz grup z dziećmi: grupy spełniającej limit (np. 1 dorosły + 3 dzieci) oraz grupy przekraczającej limit (np. 1 dorosły + 4 dzieci).