#include "pulp.h"
#include "rtc.h"

#define DS1307_ADDR_WRITE  0xD0
#define DS1307_ADDR_READ   0xD1
#define DS1307_REG_SECONDS 0x00 

// Decimal to BCD
uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// BCD to Decimal
uint8_t bcd_to_dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

int rtc_init(i2c_dev_t *dev_conf, i2c_t **i2c_handle) {
    if (dev_conf == NULL || i2c_handle == NULL) {
        return -1;
    }

    i2c_dev_init(dev_conf);
    dev_conf->id = 0; 
    dev_conf->max_baudrate = 100000; 

    *i2c_handle = i2c_open(dev_conf);
    if (*i2c_handle == NULL) {
        return -1;
    }
    return 0;
}

int rtc_set_time(i2c_t *i2c_handle, rtc_time_t *time) {
    if (i2c_handle == NULL || time == NULL) {
        return -1;
    }

    // Validasi range input sesuai datasheet DS1307
    if (time->seconds > 59 || time->minutes > 59 || time->hours > 23 ||
        time->day < 1 || time->day > 7 ||
        time->date < 1 || time->date > 31 ||
        time->month < 1 || time->month > 12 ||
        time->year > 99) {
        return -1;
    }

    unsigned char tx_data[9];

    tx_data[0] = DS1307_ADDR_WRITE; 
    tx_data[1] = DS1307_REG_SECONDS; 

    // REF DATASHEET: Register 00H (Seconds), Bit ke-7 adalah CH (Clock Halt).
    // Jika CH = 1, osilator mati. Jika CH = 0, osilator jalan.
    // Operator & 0x7F (01111111) digunakan untuk memaksa Bit-7 menjadi 0, 
    // sehingga jam dipastikan mulai berdetak setelah waktu diset.
    tx_data[2] = dec_to_bcd(time->seconds) & 0x7F; 
    
    tx_data[3] = dec_to_bcd(time->minutes);
    
    // REF DATASHEET: Register 02H (Hours), Bit ke-6 adalah flag 12/24 Mode.
    // 0 = Mode 24 jam, 1 = Mode 12 jam.
    // Operator & 0x3F (00111111) digunakan untuk memaksa Bit-6 dan Bit-7 menjadi 0, 
    // untuk memastikan sensor selalu dikunci pada format 24jam
    tx_data[4] = dec_to_bcd(time->hours) & 0x3F;
    
    tx_data[5] = dec_to_bcd(time->day);
    tx_data[6] = dec_to_bcd(time->date);
    tx_data[7] = dec_to_bcd(time->month);
    tx_data[8] = dec_to_bcd(time->year);

    // Kirim data ke DS1307 lewat I2C 
    if (i2c_write(i2c_handle, tx_data, 9, 1) != 0) {
        return -1;
    }
    return 0;
}

int rtc_get_time(i2c_t *i2c_handle, rtc_time_t *time) {
    if (i2c_handle == NULL || time == NULL) {
        return -1;
    }

    unsigned char tx_data[2];
    unsigned char rx_data[7];

    // Tahap 1: Set pointer register ke 0x00 (Register Seconds) untuk memulai burst read
    tx_data[0] = DS1307_ADDR_WRITE; 
    tx_data[1] = DS1307_REG_SECONDS; 
    if (i2c_write(i2c_handle, tx_data, 2, 0) != 0) {
        return -1;
    }

    // Tahap 2: Lakukan burst read 7 byte dari register 0x00 hingga 0x06 (Seconds, Minutes, Hours, Day, Date, Month, Year)
    tx_data[0] = DS1307_ADDR_READ; 
    if (i2c_write(i2c_handle, tx_data, 1, 0) != 0) {
        return -1;
    }
    if (i2c_read(i2c_handle, rx_data, 7, 0) != 0) {
        return -1;
    }

    // Tahap 3: Konversi hasil bacaan BCD ke Desimal
    // Potong bit CH (Clock Halt) di register detik (Bit ke-7) 
    time->seconds = bcd_to_dec(rx_data[0] & 0x7F); 
    
    time->minutes = bcd_to_dec(rx_data[1]);
    
    // Potong bit 12/24 mode (Bit 6) di register jam
    time->hours   = bcd_to_dec(rx_data[2] & 0x3F); 
    
    time->day     = bcd_to_dec(rx_data[3]);
    time->date    = bcd_to_dec(rx_data[4]);
    time->month   = bcd_to_dec(rx_data[5]);
    time->year    = bcd_to_dec(rx_data[6]);

    return 0;
}