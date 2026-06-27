PocketEngine Sprite Klasörü
===========================

Bu klasöre oyununuzun 2B sprite'larını (texture) koyun.

Desteklenen formatlar (SDL2_image üzerinden):
  .png  (önerilir — alpha kanalı destekli)
  .jpg  / .jpeg  (fotoğraf, alpha yok)
  .bmp  (sıkıştırmasız, büyük dosya)
  .tga  (alpha destekli, eski oyunlarda yaygın)
  .webp (modern, küçük boyut)
  .gif  (ilk frame only — animasyon desteklenmez)

Editör Asset Browser bu klasörü otomatik tarar:
  AssetManager::scanDirectory("assets/sprites/")

Texture yükleyici:
  IMG_Load() → SDL_ConvertSurfaceFormat(RGBA8) → glTexImage2D
  RGBA8 formatında GPU'ya upload edilir; alpha blend açık.

Öneriler:
  - PNG kullanın (alpha + lossless + küçük)
  - Sprite başına 256x256 veya 512x512 sınırı (GPU bellek)
  - Atlas/baking henüz YOK — her sprite ayrı GL texture
  - Yol: "assets/sprites/player.png"
    Inspector'da SpriteComponent.texture alanına bu path yazılır

Örnek sprite'lar (TODO — ilk release ile eklenecek):
  default_white.png   (1x1 beyaz — debug render için)
  checkerboard.png    (16x16 siyah-beyaz — viewport grid için)
