#include <M5Unified.h>
#include <ESP32DMASPIMaster.h>

constexpr int SPI_MOSI_PIN = 8;
constexpr uint32_t RESET_BYTES = 36;
constexpr uint32_t ENCODE_BYTES = 4;
constexpr uint32_t NUM_OF_LEDs = 144;
constexpr int INTERVAL_ms = 100;
constexpr size_t BUFFER_SIZE = RESET_BYTES + 3 * ENCODE_BYTES * NUM_OF_LEDs; // '3' means RGB colors

uint32_t rgbmap[NUM_OF_LEDs];

ESP32DMASPI::Master spi_master;
uint8_t *dma_tx_buf;
uint8_t *dma_rx_buf = nullptr; // Nothing to receive

// Notes
// - 3.333333MHz SPI MOSI controls WS2812B LEDs
//   = T = 0.3us
// - WS2812B LED detects following patterns as commands
//   = Reset |_____________(80+us)___________| : 266.66bits -> 288bits
//   = 0     |‾‾‾|_________| (0.3us, 0.9us)    : 4bits
//   = 1     |‾‾‾‾‾‾|______| (0.6us, 0.6us)    : 4bits
// - Each WS2812B LED requires 24bit RGB value to change colors
//   = MSB first & GRB sequence (G7, G6, ... G1, R7, R6, ..., R1, B7, B6, ... B1)
//   = SPI sends 96 bits data for each LED

// Pack 36 Bytes Reset command to 'buf'.
void PackReset(uint8_t *buf){
  for(int i=0;i<RESET_BYTES;i++){
    buf[i] = 0;
  }
}

// Pack 12 Bytes Color command to 'buf'
void PackGRB(uint8_t *buf, uint32_t rgb){
  static constexpr uint8_t lut[] = {
    0x88, // <- 00
    0x8c, // <- 01
    0xc8, // <- 10
    0xcc  // <- 11
  };
  
  buf[ 0] = lut[(rgb & 0x0000c000) >> 14];
  buf[ 1] = lut[(rgb & 0x00003000) >> 12];
  buf[ 2] = lut[(rgb & 0x00000c00) >> 10];
  buf[ 3] = lut[(rgb & 0x00000300) >>  8];
  
  buf[ 4] = lut[(rgb & 0x00c00000) >> 22];
  buf[ 5] = lut[(rgb & 0x00300000) >> 20];
  buf[ 6] = lut[(rgb & 0x000c0000) >> 18];
  buf[ 7] = lut[(rgb & 0x00030000) >> 16];
  
  buf[ 8] = lut[(rgb & 0x000000c0) >>  6];
  buf[ 9] = lut[(rgb & 0x00000030) >>  4];
  buf[10] = lut[(rgb & 0x0000000c) >>  2];
  buf[11] = lut[ rgb & 0x00000003       ];
}

void setup()
{
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  Serial.begin(115200);
  delay(1000);
  Serial.printf("RGB demo\r\n");

  dma_tx_buf = spi_master.allocDMABuffer(BUFFER_SIZE);

  spi_master.setFrequency(3333333);
  spi_master.setMaxTransferSize(BUFFER_SIZE);
  spi_master.begin(2, -1, -1, SPI_MOSI_PIN, -1); // Enable MOSI pin only
}

void UpdateColorMap(uint32_t * colormap, int offset)
{
  for(int i=0;i<NUM_OF_LEDs;i++){
    int val = (i + offset) % NUM_OF_LEDs;
    switch(val%3){
      case 0: colormap[i] = (val/2); break;
      case 1: colormap[i] = (val/2) << 8; break;
      case 2: colormap[i] = (val/2) << 16; break;
    }
  }
}

void loop()
{
  static int count = 0;
  static int prev_ms = 0;

  const int curr_ms = millis();
  const int spent_ms = curr_ms >= prev_ms ? curr_ms - prev_ms : INT_MAX - prev_ms + curr_ms;
  if(spent_ms < INTERVAL_ms){
    return;
  }

  UpdateColorMap(rgbmap, count);

  PackReset(dma_tx_buf);
  for(auto i=0; i<NUM_OF_LEDs; i++){
    PackGRB(dma_tx_buf + RESET_BYTES + i * 3 * ENCODE_BYTES, rgbmap[i]);
  }

  spi_master.transfer(dma_tx_buf, dma_rx_buf, BUFFER_SIZE);

  count++;
  prev_ms = curr_ms;
  return;
}