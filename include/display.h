//*********************************************************************************************************
const uint8_t InitArr[7][2] = { { 0x0C, 0x00 },    // display off
        { 0x00, 0xFF },    // no LEDtest
        { 0x09, 0x00 },    // BCD off
        { 0x0F, 0x00 },    // normal operation
        { 0x0B, 0x07 },    // start display
        { 0x0A, 0x04 },    // brightness
        { 0x0C, 0x01 }     // display on
};
//*********************************************************************************************************
void helpArr_init(void)  //helperarray init
{
    uint8_t i, j, k;
    j = 0;
    k = 0;
    for (i = 0; i < anzMAX * 8; i++) {
        _helpArrPos[i] = (1 << j);   //bitmask
        _helpArrMAX[i] = k;
        j++;
        if (j > 7) {
            j = 0;
            k++;
        }
    }
}
//*********************************************************************************************************
void max7219_init()  //all MAX7219 init
{
    uint8_t i, j;
    for (i = 0; i < 7; i++) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(InitArr[i][0]);  //register
            SPI.write(InitArr[i][1]);  //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void max7219_set_brightness(unsigned short br)  //brightness MAX7219
{
    uint8_t j;
    if (br < 16) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(0x0A);  //register
            SPI.write(br);    //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void clear_Display()   //clear all
{
    uint8_t i, j;
    for (i = 0; i < 8; i++)     //8 rows
    {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = anzMAX; j > 0; j--) {
            _LEDarr[j - 1][i] = 0;       //LEDarr clear
            SPI.write(i + 1);           //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void refresh_display() //take info into LEDarr
{
    uint8_t i, j;

    for (i = 0; i < 8; i++)     //8 rows
    {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = anzMAX; j > 0; j--) {
            SPI.write(i + 1);  //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************