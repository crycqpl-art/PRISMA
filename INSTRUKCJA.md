# PRISMA v0.1 – instrukcja

Prisma to **instrument** do Abletona (nie efekt). Wrzucasz do niego do 4 sampli, grasz nuty z klawiatury MIDI, a wtyczka tnie i miesza sample w tempie projektu.

## Szybki start
1. Utwórz ścieżkę MIDI i wrzuć na nią **Prisma** (Plug-Ins → Kamil Potebski → Prisma).
2. Przeciągnij plik audio (albo klip prosto z aranżacji Abletona) na slot **A**. Kolejne pliki daj na B, C i D.
3. Zagraj nutę. **C3 (nuta 60) to oryginalna wysokość**, wyższe nuty są wyżej, niższe niżej.
4. Wybierz preset z listy **Presets** albo klikaj **Dice**.

## Sekcje

**Sloty A–D** – przeciągnij plik albo kliknij *Load*, a *x* czyści slot. Kliknięcie slotu pokazuje go w dużym podglądzie. Kreski na dole podglądu to wykryte uderzenia.

**Duży podgląd** – kliknięcie albo przeciągnięcie ustawia **Position**. Biała linia pokazuje, skąd teraz grane jest audio, jasne pole to zakres skakania (Range), a złote kreski to miejsca, w które właśnie „wskoczył” chop.

**Morph (kwadrat po prawej)** – przeciągaj kropkę między A, B, C i D, żeby mieszać sample.

**PLAY**
- *Mode*: **Offset** (skakanie po samplu w tempie) albo **Slices** (każdy klawisz od C1 w górę to kolejny pocięty kawałek).
- *Position*: od którego miejsca sampla zaczynamy.
- *Scan*: jak szybko punkt grania jedzie przez sampel (0 = stoi, 1x = normalnie, minus = do tyłu).

**OFFSET** (to, co we FL robiło się LFO na „sample start offset”)
- *Rate*: jak często sampel startuje od nowa (1/4 … 1/32, T = triole).
- *Shape*: jak skacze punkt startu. **Random** losowo, **Ramp** po kolei, **Sine** falując, **Drift** błądząc powoli.
- *Range*: jak daleko może skoczyć (0 = zawsze to samo miejsce, czyli stutter).
- *Snap*: przyciąganie skoków do uderzeń i sylab, dzięki czemu chopy brzmią muzycznie.
- *Length*: jak długo gra każdy kawałek (krótko = staccato, glitch).
- *Probability*: szansa, że uderzenie zagra (niżej = dziury w rytmie).
- *Reverse*: szansa, że kawałek zagra od tyłu.

**PITCH + MORPH**
- *Pitch set* + *Pitch rand*: losowa zmiana wysokości kawałków w wybranej skali (oktawy, kwinty, moll, dur, pentatonika, chaos).
- *Slot random*: wyłączone = każdy chop miesza wszystkie sloty według kropki; włączone = każdy chop losuje jeden slot.

**CLOUD** – granulacja: z kawałków robi się chmura drobinek. *Cloud* to ile chmury, *Size* długość drobinek, *Density* ile ich na sekundę, *Spread* jak szeroko je rozrzuca.

**AMP** – *Attack* i *Release* nuty.

**COLOR**
- *Crunch*: przester, a przy wysokich wartościach cyfrowe „zniszczenie”.
- *Wash*: zamglenie, czyli ciemniejszy dźwięk, duży pogłos i trzaski winyla.
- *Pump*: pompowanie w rytmie ćwierćnut.

## Jak przekazywać mi zmiany (najszybszy sposób)
1. **Copy settings** – kopiuje wszystkie ustawienia jako tekst. Wklej go do czatu i napisz, co brzmi źle albo jak ma brzmieć. Wtedy widzę dokładnie to samo co Ty.
2. **Paste settings** – działa w drugą stronę: ja wysyłam Ci tekst z ustawieniami, kopiujesz go i klikasz Paste, a wtyczka ustawia się sama.
3. **Numer wersji** jest w lewym górnym rogu (np. v0.1.0). Zawsze pisz, którą wersję testujesz.
4. **Dziel uwagi na 3 grupy:** *Błędy* (coś nie działa, trzeszczy, wysypuje się), *Brzmienie* (co ma brzmieć inaczej), *Wygląd i obsługa*.
5. **Opisuj dźwięk przykładem:** „ma brzmieć jak wokal w [utwór] w 1:12” albo nagraj 10 sekund (Twój telefon lub eksport z Abletona) i wrzuć do czatu.
6. **Zrzut ekranu z dopiskiem** („ta gałka za mało zmienia”) mówi więcej niż długi opis.

## Instalacja (Mac)
1. Pobierz `Prisma-macOS` z GitHuba (Actions → przebieg → Artifacts) i rozpakuj.
2. W Terminalu:
   ```
   mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
   cp -R ~/Downloads/Prisma-macOS/Prisma.vst3 ~/Library/Audio/Plug-Ins/VST3/
   cp -R ~/Downloads/Prisma-macOS/Prisma.component ~/Library/Audio/Plug-Ins/Components/
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Prisma.vst3 ~/Library/Audio/Plug-Ins/Components/Prisma.component
   ```
3. Ableton → Settings → Plug-Ins → przytrzymaj Option i kliknij **Rescan**.

Uwaga: wtyczka zapamiętuje, **gdzie leżą pliki** sampli. Jeśli je przeniesiesz, slot pokaże „Missing” i trzeba wczytać plik ponownie.
