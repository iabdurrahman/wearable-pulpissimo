# Kalibrasi Aux ADC LIS3DHTR — Channel Baterai (ADC1)

Dokumen ini mencatat metodologi dan data kalibrasi di balik konstanta
`1283.5f` pada `lis3dhtr_raw_to_millivolt()` di `lis3dhtr.c`. Tujuannya
supaya kalibrasi bisa direproduksi/diverifikasi ulang tanpa perlu
mengulang dari nol, dan supaya batasannya jelas untuk pemakai driver
di masa depan.


---

## 1. Latar belakang masalah

Formula datasheet ST untuk konversi raw ADC aux → mV:

```
mV = 1200 + (raw × mv_per_digit)
mv_per_digit (10-bit) = 800 / 1024 = 0.78125 mV/LSB
```

Dua penyimpangan ditemukan lewat pengujian di hardware asli (board
ICDeC PULPissimo, chip LIS3DHTR, alamat I2C 0x19):

1. **Arah/polaritas terbalik.** Tegangan input makin tinggi →
   raw makin NEGATIF (bukan makin positif seperti asumsi awal).
   Dikonfirmasi dari uji channel TP4056 (ADC2): idle (~0V setelah
   divider) → raw ≈ +508; charging (~1.6V, clip) → raw ≈ -508.
2. **Offset/bias 1200 mV tidak akurat** untuk unit chip ini. Dengan
   rumus offset 1200 dan tanda dibalik, hasil masih meleset jauh dari
   nilai aktual (multimeter 4.130V vs hasil hitung ~3.33V pada kasus
   awal — selisih ~0.8V / ~19%, terlalu besar untuk toleransi ADC
   normal).

## 2. Rumus hasil kalibrasi

```c
mV = 1283.5f - (raw × mv_per_digit)
```

- **Bias**: `1200.0` → `1283.5` mV (dikalibrasi ulang).
- **Slope (mv_per_digit)**: **tidak diubah**, tetap `800/1024 =
  0.78125 mV/LSB` sesuai datasheet, dengan tanda dibalik (dikurangi,
  bukan ditambah) untuk merepresentasikan polaritas terbalik.
- **Rasio divider** (`47/141`, diterapkan di layer `lis3dhtr_read_voltage*`,
  bukan di fungsi ini) tidak berubah — sudah dikonfirmasi benar dari
  desain hardware, bukan bagian yang dikalibrasi di sini.

### Mengapa cuma bias yang disesuaikan, bukan slope?

**Perlu diverifikasi dari data mentah di bagian 4** — tapi hipotesis
kerja saat ini: slope ADC (rasio 800mV/1024 count) ditentukan oleh
referensi tegangan internal chip yang biasanya di-trim presisi saat
fabrikasi dan konsisten antar-unit, sehingga cenderung cocok dengan
datasheet apa adanya. Offset/bias lebih rentan bergeser antar-unit
akibat toleransi manufaktur dan kondisi board (mis. Vdd aktual board
ICDeC ini kemungkinan tidak persis sama dengan kondisi karakterisasi
ST di datasheet, yang diasumsikan Vdd≈2.5V).

**Ini baru hipotesis, bukan fakta yang sudah dibuktikan** — untuk
mengonfirmasi, perlu dilakukan regresi 2-parameter (slope DAN bias
sama-sama bebas, tidak difiksasi ke nilai datasheet) atas data mentah,
lalu bandingkan slope hasil fit dengan 0.78125. Kalau hasil fit
slope-nya jelas berbeda signifikan dari 0.78125 tapi kode ini tetap
memakai slope datasheet, berarti model saat ini bukan hasil regresi
optimal — MSE (mean squared error) sisa kemungkinan bisa ditekan lebih
jauh lagi dengan slope yang disesuaikan juga.

## 3. Rentang validitas

| Parameter | Rentang tervalidasi | Catatan |
|---|---|---|
| Tegangan sumber (sebelum divider) | 3.0 V – 4.2 V | Rentang operasi baterai Li-ion 1S |
| Tegangan di pin ADC (setelah divider 47/141) | ≈1000 – 1400 mV | `3.0×(47/141)` s.d. `4.2×(47/141)` |
| Channel | ADC1 (CH1) saja | Belum divalidasi terpisah utk CH2/CH3 |
| Resolusi | 10-bit (`LIS3DHTR_ADC_RES_10BIT`) | Belum divalidasi utk mode 8-bit |
| Suhu | (isi kondisi ruang pengujian) | Karakteristik ADC bisa bergeser dgn suhu |

**Di luar rentang ini, akurasi TIDAK terjamin.** Contoh: channel
TP4056 (ADC2) sengaja dibiarkan clip di ~800mV/~1600mV — di luar
rentang tervalidasi 1000-1400mV — tapi ini aman karena logic TP4056
cuma memakai tanda raw (positif/negatif) untuk deteksi biner, bukan
nilai mV presisi.

## 4. Data mentah

Sumber: 13 level tegangan referensi (3.0V–4.2V, step 0.1V), tiap level
diambil ~9-10 replikasi dengan interval 15 detik antar-sample (bukan
burst cepat — ini mengurangi risiko yang disebut di section 3
soal replikasi tidak independen). Kolom "Terbaca (lama)" dan
"Error % (lama)" dihitung pakai formula SEBELUM kalibrasi (offset
1200mV) — lihat catatan status di atas dokumen.

| V masuk (V) | raw ADC (semua replikasi) | raw rata-rata | Terbaca lama, contoh (V) | Error % lama, kisaran |
|---|---|---|---|---|
| 4.2 | -145,-150,-142,-145,-145,-146,-145,-145,-144,-146 | -145.3 | 3.939 | 5.9 – 6.4 |
| 4.1 | -108,-107,-106,-106,-109,-106,-107,-105,-106,-105 | -106.5 | 3.850 | 6.0 – 6.2 |
| 4.0 | -63,-62,-63,-67,-61,-62,-62,-62,-64,-62 | -62.8 | 3.746 | 6.1 – 6.5 |
| 3.9 | -20,-22,-20,-20,-20,-19,-18,-20,-19,-19 | -19.7 | 3.645 | 6.4 – 6.6 |
| 3.8 | 23,21,22,21,24,23,22,23,22,23 | 22.4 | 3.547 | 6.6 – 6.8 |
| 3.7 | 66,63,66,66,67,64,64,65,63 | 64.9 | 3.450 | 6.6 – 7.0 |
| 3.6 | 106,107,104,106,107,108,107,106,108 | 106.6 | 3.349 | 6.8 – 7.1 |
| 3.5 | 147,150,149,147,149,149,148,149 | 148.5 | 3.251 | 7.0 – 7.2 |
| 3.4 | 190,191,190,191,192,190,191,190,190 | 190.6 | 3.153 | 7.2 – 7.4 |
| 3.3 | 235,234,234,235,235,234,235,234,233 | 234.3 | 3.050 | 7.5 – 7.6 |
| 3.2 | 278,276,277,276,277,277,275,278,274,276 | 276.4 | 2.952 | 7.6 – 7.9 |
| 3.1 | 318,318,319,321,320,320,318,318,319 | 319.0 | 2.852 | 7.9 – 8.2 |
| 3.0 | 359,361,362,361,360,363,361,362,361,360 | 361.0 | 2.753 | 8.1 – 8.4 |

Total sample: **~124 titik** (beberapa level punya 8-10 replikasi,
bukan genap 10 semua — lihat kolom raw untuk jumlah pasti per level).

### Hasil regresi linear (dihitung dari 13 titik rata-rata per level)

Model: `mV_pin = A + B × raw`, dengan `mV_pin = V_masuk × 1000/3`
(rasio divider 47/141 = tepat 1/3, jadi konversi ini eksak).

```
mean(raw)   = 106.876
mean(mV)    = 1200.000   (sesuai ekspektasi, midpoint dari 3.0-4.2V)

B (slope)  = Σ(dx·dy) / Σ(dx²) = -256803.9 / 326145.6 ≈ -0.7874 mV/LSB
A (bias)   = mean(mV) - B×mean(raw) ≈ 1200.0 + 0.7874×106.876 ≈ 1284.2 mV

R² = (Σdx·dy)² / (Σdx² × Σdy²) ≈ 0.9998
```

**Interpretasi:**
- Slope hasil fit (-0.7874) vs slope datasheet yang dipakai di kode
  (-0.78125): selisih ~0.8%. Ini mengonfirmasi keputusan menahan
  slope tetap ke nilai datasheet — dampak sistematik dari selisih ini
  di titik terjauh dari mean (~raw=-145 atau raw=361) cuma sekitar
  1.5mV, jauh di bawah signifikan dibanding skala pengukuran (~1000mV).
- Bias hasil fit (1284.2 mV) sangat dekat dengan 1283.5 mV yang
  dipakai di kode — beda 0.7mV, dalam batas wajar noise pengukuran.
- R²≈0.9998 di sini dihitung dari 13 titik **rata-rata per level**
  (bukan seluruh ~124 titik individual). Rata-rata per level
  menghaluskan noise dalam 1 level, jadi R² dari 124 titik mentah
  kemungkinan sedikit lebih rendah dari 0.9998, tapi mengingat spread
  dalam 1 level cuma beberapa LSB (misal level 4.2V: rentang raw
  142-150, cuma 8 LSB) dibanding rentang total ~506 LSB antar level,
  R² dari data mentah penuh seharusnya tetap sangat tinggi (>0.999).

### Cara hitung R² dari tabel di atas

```
R² = 1 - (SS_res / SS_tot)

SS_res = Σ (V_aktual_i - V_prediksi_i)²
SS_tot = Σ (V_aktual_i - V_rata_rata)²
```

- `V_prediksi_i` = hasil rumus kalibrasi untuk `raw_i` (kolom mV
  dikonversi ke V, lalu dibagi rasio divider — atau langsung
  bandingkan di level sebelum divider, konsisten pilih salah satu).
- `V_rata_rata` = rata-rata seluruh `V_aktual` di tabel.
- **Peringatan validitas statistik**: kalau 10 replikasi per level
  diambil berurutan cepat tanpa jeda (lihat catatan di
  `lis3dhtr_read_raw_avg()` soal keterbatasan tanpa API timer
  portable), replikasi tsb kemungkinan BUKAN sample independen secara
  statistik (bisa jadi nilai chip yang sama terbaca berulang). Ini
  bisa membuat R² tampak lebih tinggi dari yang seharusnya
  merepresentasikan noise asli. Idealnya re-ukur dengan jeda antar
  replikasi (mis. delay dari sisi aplikasi) sebelum angka R² final
  ini dipakai sebagai klaim akurasi di dokumentasi resmi.

## 5. Error rumus BARU (offset 1283.5) — dihitung ulang dari data section 4

`V_prediksi = (1283.5 - raw×0.78125) × 3 / 1000`, dihitung memakai
raw rata-rata tiap level (bukan disalin dari klaim komentar kode):

| V masuk (V) | raw rata-rata | V prediksi (rumus baru) | Error (%) |
|---|---|---|---|
| 4.2 | -145.3 | 4.191 | 0.21 |
| 4.1 | -106.5 | 4.100 | 0.00 |
| 4.0 | -62.8 | 3.998 | 0.06 |
| 3.9 | -19.7 | 3.897 | 0.09 |
| 3.8 | 22.4 | 3.798 | 0.05 |
| 3.7 | 64.9 | 3.698 | 0.04 |
| 3.6 | 106.6 | 3.601 | 0.02 |
| 3.5 | 148.5 | 3.502 | 0.07 |
| 3.4 | 190.6 | 3.404 | 0.11 |
| 3.3 | 234.3 | 3.301 | 0.04 |
| 3.2 | 276.4 | 3.203 | 0.08 |
| 3.1 | 319.0 | 3.103 | 0.09 |
| 3.0 | 361.0 | 3.004 | 0.15 |

**Error maks di sini (berbasis rata-rata per level): 0.21%**, terjadi
di level 4.2V. Ini di bawah klaim 0.77% di komentar kode — kemungkinan
0.77% berasal dari titik raw INDIVIDUAL yang menyimpang paling jauh
dari rata-rata levelnya (bukan rata-rata), misal raw=-142 di level
4.2V (bukan -150) memberi error individual lebih besar dari rata-rata
level. **Ini belum sepenuhnya tertelusuri** — kalau mau dokumentasi
100% presisi, hitung error untuk SEMUA ~124 titik individual (bukan
cuma 13 rata-rata level) dan catat titik mana persis yang memberi
0.77%. Baris di atas sudah cukup untuk menunjukkan orde besaran error
(<0.3% konsisten), tapi bukan pengganti perhitungan penuh 124 titik.

### Perbandingan dengan kejadian awal di sesi ini

| Kondisi | raw | V_aktual (multimeter) | V rumus LAMA (offset 1200) | V rumus BARU (offset 1283.5) |
|---|---|---|---|---|
| Baterai ~27%, uji awal | -116 | 4.130 V | ≈3.328 V (error ~19.4%) | ≈4.122 V (error ≈0.19%) |

