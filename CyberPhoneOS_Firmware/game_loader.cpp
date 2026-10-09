#include "game_loader.h"
#include "config.h"
#include <SD.h>

void game_loader_scan(GameList &out) {
  out.count = 0;

  File root = SD.open(SD_DIR_GAMES);
  if (!root || !root.isDirectory()) return;

  File entry = root.openNextFile();
  while (entry && out.count < MAX_GAMES) {
    if (entry.isDirectory()) {
      // basename() equivalent: strip the "/games/" prefix
      String full = String(entry.name());
      int slash = full.lastIndexOf('/');
      String base = (slash >= 0) ? full.substring(slash + 1) : full;
      strlcpy(out.games[out.count].folderName, base.c_str(), sizeof(out.games[out.count].folderName));
      out.count++;
    }
    // Must close before advancing, otherwise the open handles pile up and
    // SD.h eventually refuses to open new files (max ~16 concurrent handles).
    entry.close();
    entry = root.openNextFile();
  }
  if (entry) entry.close();
  root.close();
}

int game_loader_get_highscore(const char *folder) {
  String path = String(SD_DIR_GAMES) + "/" + folder + "/highscore.txt";
  if (!SD.exists(path)) return 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return 0;
  int score = f.parseInt();
  f.close();
  return score;
}

void game_loader_set_highscore(const char *folder, int score) {
  String dir = String(SD_DIR_GAMES) + "/" + folder;
  if (!SD.exists(dir)) SD.mkdir(dir);
  String path = dir + "/highscore.txt";
  File f = SD.open(path, FILE_WRITE);
  if (!f) return;
  f.print(score);
  f.close();
}
