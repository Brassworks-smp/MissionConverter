# Brassworks SMP Mission Converter

This script converts the official BrassworksSMP missions Google Sheet into a `missions.json` file for use with the mod, Now written in cpp cuz why not.

## Official Missions Sheet

The official list of missions is maintained here:
[https://docs.google.com/spreadsheets/d/1g_Fn5qVjEgfV0PsRR6tH91PJeQkH-wexDphUM6nU804/edit?usp=sharing](https://docs.google.com/spreadsheets/d/1g_Fn5qVjEgfV0PsRR6tH91PJeQkH-wexDphUM6nU804/edit?usp=sharing)

## Setup

1.  **Item List:** To get an updated list of all valid item IDs, join the Brassworks SMP server (or a singleplayer world with the modpack) and run the command:
    `/brassworks dumpitems`
    This will create a file named `itemlist_dump.txt` in ur main mineacraft foler. Place this file in the same directory as the Python script.

2.  **Configuration:** You can change the script's parameters (like the Google Sheet URL or output file name) by editing the constants at the top of the `convert_missions.py` file.

3.  **Run:** Execute the script to generate the `missions.json` file.
