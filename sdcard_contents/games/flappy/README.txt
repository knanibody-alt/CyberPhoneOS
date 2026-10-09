Bu klasor "Oyunlar" menusunde otomatik olarak listelenir (SD:/games altindaki
her klasor dinamik olarak taranir - bkz. game_loader.cpp).

Şu an yalnizca "snake" klasoru icin gercek bir oyun motoru var (LVGL ile
ESP32 uzerinde calisan yerlesik Yilan oyunu). "flappy" burada bir ORNEK
olarak duruyor: listede gorunur ve high score dosyasi okunur/yazilir, ama
dokunulunca "Bu oyun icin motor henuz yazilmadi" mesaji cikar.

Kendi Flappy Bird motorunuzu eklemek isterseniz:
  1. ui_manager.cpp icindeki game_entry_cb() fonksiyonuna "flappy" icin
     ozel bir dal ekleyin (snake icin yapildigi gibi).
  2. Kus/boru gorsellerini bu klasore PNG olarak degil, LVGL'in
     lv_img_dsc_t formatina donusturulmus C dizileri olarak ekleyin
     (LVGL'in kendi "Image converter" araciyla - ham/binary format ekrana
     doğrudan pushColors ile basilamaz).
  3. highscore.txt otomatik olarak game_loader_get/set_highscore()
     tarafindan okunup yazilir, degistirmeniz gerekmez.
