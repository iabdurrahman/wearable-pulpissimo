#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Copy of BCD conversion functions untuk test
uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

uint8_t bcd_to_dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} rtc_time_t;

// SIMULASI DS1307 I2C DEVICE (MEMORY)
typedef struct {
    uint8_t registers[7];  // 7 register waktu DS1307
    uint8_t is_initialized;
} rtc_simulator_t;

rtc_simulator_t rtc_sim = {0};

// Simulasi I2C WRITE ke DS1307 (set time)
void sim_rtc_write(uint8_t *data, int len) {
    printf("\n  [I2C WRITE] Mengirim %d bytes ke DS1307:\n", len);
    for (int i = 0; i < len; i++) {
        printf("    Byte %d: 0x%02x\n", i, data[i]);
    }
    
    // Asumsi format: data[0]=addr_write (0x68<<1=0xD0), data[1]=0x00 (reg), data[2..8]=values
    if (len == 9 && data[0] == (0x68 << 1) && data[1] == 0x00) {
        // Simpan 7 byte register
        for (int i = 0; i < 7; i++) {
            rtc_sim.registers[i] = data[i + 2];
        }
        rtc_sim.is_initialized = 1;
        printf("  [✓] Data tersimpan di memory simulasi DS1307\n");
    }
}

// Simulasi I2C READ dari DS1307 (get time)
void sim_rtc_read(uint8_t *data, int len) {
    printf("\n  [I2C READ] Membaca %d bytes dari DS1307:\n", len);
    
    if (rtc_sim.is_initialized && len == 7) {
        for (int i = 0; i < 7; i++) {
            data[i] = rtc_sim.registers[i];
            printf("    Byte %d: 0x%02x (BCD)\n", i, data[i]);
        }
        printf("  [✓] Data berhasil dibaca dari simulasi\n");
    } else {
        printf("  [⚠] Simulasi belum diinisialisasi!\n");
    }
}

void test_bcd_conversion(void) {
    printf("\n   RTC DS1307 Unit Test - BCD Conversion Test   \n");
    
    printf("\n=== TEST 1: Decimal to BCD Conversion ===\n");
    
    struct {
        uint8_t dec;
        uint8_t expected_bcd;
        const char *desc;
    } test_cases[] = {
        {0, 0x00, "Nol"},
        {9, 0x09, "Satu digit"},
        {10, 0x10, "10"},
        {15, 0x15, "15"},
        {30, 0x30, "30"},
        {59, 0x59, "59"},
        {23, 0x23, "23 (Jam)"},
    };
    
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int pass = 0, fail = 0;
    
    for (int i = 0; i < num_tests; i++) {
        uint8_t result = dec_to_bcd(test_cases[i].dec);
        if (result == test_cases[i].expected_bcd) {
            printf("  [✓] dec_to_bcd(%2d [%-15s]) = 0x%02x\n", 
                   test_cases[i].dec, test_cases[i].desc, result);
            pass++;
        } else {
            printf("  [✗] dec_to_bcd(%2d [%-15s]) = 0x%02x (expected 0x%02x)\n",
                   test_cases[i].dec, test_cases[i].desc, result, test_cases[i].expected_bcd);
            fail++;
        }
    }
    
    printf("\n=== TEST 2: BCD to Decimal Conversion ===\n");
    
    struct {
        uint8_t bcd;
        uint8_t expected_dec;
        const char *desc;
    } test_cases_bcd2dec[] = {
        {0x00, 0, "Nol"},
        {0x09, 9, "Satu digit"},
        {0x10, 10, "10"},
        {0x15, 15, "15"},
        {0x30, 30, "30"},
        {0x59, 59, "59"},
        {0x23, 23, "23 (Jam)"},
    };
    
    int num_tests_bcd = sizeof(test_cases_bcd2dec) / sizeof(test_cases_bcd2dec[0]);
    
    for (int i = 0; i < num_tests_bcd; i++) {
        uint8_t result = bcd_to_dec(test_cases_bcd2dec[i].bcd);
        if (result == test_cases_bcd2dec[i].expected_dec) {
            printf("  [✓] bcd_to_dec(0x%02x [%-15s]) = %2d\n",
                   test_cases_bcd2dec[i].bcd, test_cases_bcd2dec[i].desc, result);
            pass++;
        } else {
            printf("  [✗] bcd_to_dec(0x%02x [%-15s]) = %2d (expected %d)\n",
                   test_cases_bcd2dec[i].bcd, test_cases_bcd2dec[i].desc, result, test_cases_bcd2dec[i].expected_dec);
            fail++;
        }
    }
    
    printf("\n=== TEST 3: Time Structure Validation ===\n");
    
    rtc_time_t time;
    time.seconds = 45;
    time.minutes = 30;
    time.hours = 14;
    time.day = 4;
    time.date = 24;
    time.month = 6;
    time.year = 26;
    
    printf("  Set Waktu Asli:\n");
    printf("    Jam:Menit:Detik = %02d:%02d:%02d\n", time.hours, time.minutes, time.seconds);
    printf("    Tanggal: %02d/%02d/20%02d\n", time.date, time.month, time.year);
    printf("    Hari: %d (0=Min, 1=Sen, 2=Sel, 3=Rab, 4=Kam, 5=Jum, 6=Sab)\n\n", time.day);
    
    // Konversi ke BCD dan kembali ke decimal
    uint8_t bcd_sec = dec_to_bcd(time.seconds);
    uint8_t bcd_min = dec_to_bcd(time.minutes);
    uint8_t bcd_hour = dec_to_bcd(time.hours);
    
    uint8_t dec_sec = bcd_to_dec(bcd_sec);
    uint8_t dec_min = bcd_to_dec(bcd_min);
    uint8_t dec_hour = bcd_to_dec(bcd_hour);
    
    printf("  Konversi Nilai:\n");
    printf("    Detik:  %02d → 0x%02x → %02d %s\n", time.seconds, bcd_sec, dec_sec, 
           (dec_sec == time.seconds) ? "[✓]" : "[✗]");
    printf("    Menit:  %02d → 0x%02x → %02d %s\n", time.minutes, bcd_min, dec_min,
           (dec_min == time.minutes) ? "[✓]" : "[✗]");
    printf("    Jam:    %02d → 0x%02x → %02d %s\n", time.hours, bcd_hour, dec_hour,
           (dec_hour == time.hours) ? "[✓]" : "[✗]");
    
    if (dec_sec == time.seconds && dec_min == time.minutes && dec_hour == time.hours) {
        printf("\n  [✓] Round-trip conversion OK!\n");
        pass++;
    } else {
        printf("\n  [✗] Round-trip conversion FAILED!\n");
        fail++;
    }
    
    printf("\n════════════════════════════════════════════════\n");
    printf("SUMMARY:\n");
    printf("  Total Tests: %d\n", pass + fail);
    printf("  Passed:      %d ✓\n", pass);
    printf("  Failed:      %d ✗\n", fail);
    printf("\n  Result: %s\n", fail == 0 ? "ALL TESTS PASSED! ✓" : "SOME TESTS FAILED! ✗");
    printf("════════════════════════════════════════════════\n\n");
    
    if (fail == 0) {
        printf("READY FOR HARDWARE TEST!\n");
        printf("Next: Flash ke board FPGA dan test dengan I2C device DS1307\n");
    }
}

void test_rtc_simulator(void) {
    printf("\n=== TEST 4: DS1307 I2C Simulator ===\n");
    printf("\nSkenario: Set waktu ke DS1307, lalu baca kembali\n");
    
    // Step 1: Siapkan waktu
    rtc_time_t waktu_awal = {
        .seconds = 0,
        .minutes = 30,
        .hours = 14,
        .day = 4,
        .date = 25,
        .month = 6,
        .year = 26
    };
    
    printf("\n--- STEP 1: Prepare Data ---\n");
    printf("Waktu yang akan di-SET ke DS1307:\n");
    printf("  Jam:Menit:Detik = %02d:%02d:%02d\n", waktu_awal.hours, waktu_awal.minutes, waktu_awal.seconds);
    printf("  Tanggal: %02d/%02d/20%02d\n", waktu_awal.date, waktu_awal.month, waktu_awal.year);
    printf("  Hari: %d (Rabu)\n\n", waktu_awal.day);
    
    // Step 2: Convert ke BCD
    printf("--- STEP 2: Convert to BCD Format ---\n");
    uint8_t tx_data[9];
    tx_data[0] = 0x68 << 1;  // 7-bit address DS1307 di-shift ke 8-bit (= 0xD0)
    tx_data[1] = 0x00;
    tx_data[2] = dec_to_bcd(waktu_awal.seconds) & 0x7F;
    tx_data[3] = dec_to_bcd(waktu_awal.minutes);
    tx_data[4] = dec_to_bcd(waktu_awal.hours);
    tx_data[5] = dec_to_bcd(waktu_awal.day);
    tx_data[6] = dec_to_bcd(waktu_awal.date);
    tx_data[7] = dec_to_bcd(waktu_awal.month);
    tx_data[8] = dec_to_bcd(waktu_awal.year);
    
    printf("Data BCD untuk dikirim: ");
    for (int i = 2; i < 9; i++) printf("0x%02x ", tx_data[i]);
    printf("\n\n");
    
    // Step 3: I2C WRITE
    printf("--- STEP 3: I2C WRITE (Set waktu ke DS1307) ---\n");
    sim_rtc_write(tx_data, 9);
    
    // Step 4: I2C READ
    printf("\n--- STEP 4: I2C READ (Baca waktu dari DS1307) ---\n");
    uint8_t rx_data[7];
    sim_rtc_read(rx_data, 7);
    
    // Step 5: Convert BCD ke decimal
    printf("\n--- STEP 5: Convert BCD to Decimal ---\n");
    rtc_time_t waktu_baca = {
        .seconds = bcd_to_dec(rx_data[0] & 0x7F),
        .minutes = bcd_to_dec(rx_data[1]),
        .hours   = bcd_to_dec(rx_data[2]),
        .day     = bcd_to_dec(rx_data[3]),
        .date    = bcd_to_dec(rx_data[4]),
        .month   = bcd_to_dec(rx_data[5]),
        .year    = bcd_to_dec(rx_data[6])
    };
    
    // Step 6: Verify
    printf("\n--- STEP 6: Verify (Cek kecocokan) ---\n");
    int match = (waktu_baca.seconds == waktu_awal.seconds) &&
                (waktu_baca.minutes == waktu_awal.minutes) &&
                (waktu_baca.hours == waktu_awal.hours) &&
                (waktu_baca.date == waktu_awal.date) &&
                (waktu_baca.month == waktu_awal.month) &&
                (waktu_baca.year == waktu_awal.year);
    
    if (match) {
        printf("  [✓] SEMUA COCOK! Data terkirim dan terbaca dengan benar.\n");
    } else {
        printf("  [✗] Ada yang tidak cocok\n");
    }
    
    printf("\n--- HASIL AKHIR ---\n");
    printf("Waktu dari DS1307: %02d:%02d:%02d | %02d/%02d/20%02d\n",
           waktu_baca.hours, waktu_baca.minutes, waktu_baca.seconds,
           waktu_baca.date, waktu_baca.month, waktu_baca.year);
}

int main(void) {
    test_bcd_conversion();
    test_rtc_simulator();
    return 0;
}
