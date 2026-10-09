#ifndef GAME_LOADER_H
#define GAME_LOADER_H

#include <Arduino.h>

#define MAX_GAMES 12

struct GameEntry {
  char folderName[24];   // e.g. "snake" -> used to look up assets/highscore
};

struct GameList {
  GameEntry games[MAX_GAMES];
  int count;
};

// Scans /games on the SD card and returns every subfolder found. This is
// what lets you drop a new folder onto the card and have it appear in the
// launcher without reflashing firmware (only "snake" has a built-in engine
// right now - see ui_manager.cpp screen_games_open_game()).
void game_loader_scan(GameList &out);

// Reads/writes a simple integer high score at /games/<folder>/highscore.txt
int  game_loader_get_highscore(const char *folder);
void game_loader_set_highscore(const char *folder, int score);

#endif // GAME_LOADER_H
