PocketEngine Font Klasörü
=========================

Bu klasöre ImGui editör için kullanılacak TTF fontu konur.

Varsayılan font: Roboto-Medium.ttf

Kurulum:
  setup-termux.sh scripti bu fontu otomatik olarak indirir:
    https://github.com/googlefonts/roboto/raw/main/src/hinted/Roboto-Medium.ttf

  İndirme başarısız olursa:
    1. Elle indirip bu klasöre Roboto-Medium.ttf olarak koyun
    2. Veya farklı bir .ttf dosyası koyun (ismin önemi yok,
       editör klasördeki ilk .ttf dosyasını yükler)

  Alternatif URL'ler:
    https://github.com/googlefonts/roboto-3-classic/raw/main/src/hinted/Roboto-Medium.ttf
    https://fonts.google.com/specimen/Roboto

Format: TTF (TrueType) veya OTF (OpenType)
Boyut: 16-24 pt arası önerilir (ImGui 18 pt default)
Lisans: Roboto Apache 2.0 (ticari kullanıma uygun)

Not: Font yoksa ImGui programatik default fontunu kullanır (noktasal,
ekranı doldurur). Editör deneyimi için bir font şarttır.
