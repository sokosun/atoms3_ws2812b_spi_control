#include <M5Unified.h>
#include <ESP32DMASPIMaster.h>

const int SPI_MOSI_PIN = 8;
const uint32_t NUM_OF_LEDs = 144;
const int INTERVAL_ms = 100;
uint32_t rgbmap[NUM_OF_LEDs];

ESP32DMASPI::Master spi_master;
static constexpr size_t BUFFER_SIZE = 5284; // (Reset) 100Bytes + (RGB) 3Bytes * (encode) 12 * 144LEDs
uint8_t *dma_tx_buf;
uint8_t *dma_rx_buf = nullptr; // Nothing to receive

// Notes
// - 10MHz SPI MOSI controls WS2812B LEDs
//   = T = 0.1us
// - WS2812B LED detects following patterns as commands
//   = Reset |_____________(80us)____________| : 800bits
//   = 0     |‾‾‾‾‾‾‾‾‾|___| (0.9us, 0.3us)    : 12bits
//   = 1     |‾‾‾‾‾‾|______| (0.6us, 0.6us)    : 12bits
// - Each WS2812B LED requires 24bit RGB value to change colors
//   = MSB first & GRB sequence (G7, G6, ... G1, R7, R6, ..., R1, B7, B6, ... B1)
//   = SPI sends 288 bits data for each LED

// Pack 100 Bytes Reset command to 'buf'.
void PackReset(uint8_t *buf){
  for(int i=0;i<100;i++){
    buf[i] = 0;
  }
}

// Encode 'val' (2bits) to 3Bytes pattern and Pack it.
// 'val' must be 0, 1, 2 or 3.
// // Each bit is converted to 12bit. (0 -> 0xe00, 1 -> 0xfc0)
void PackGRBsub(uint8_t *buf, uint8_t val){
  const uint8_t lut_x12[] = {
    0xe0, 0x0e, 0x00, // <- 00
    0xe0, 0x0f, 0xc0, // <- 01
    0xfc, 0x0e, 0x00, // <- 10
    0xfc, 0x0f, 0xc0  // <- 11
  };

  buf[0] = lut_x12[0 + val*3];
  buf[1] = lut_x12[1 + val*3];
  buf[2] = lut_x12[2 + val*3];
}

// Pack 36 Bytes Color command to 'buf'.
void PackGRB(uint8_t *buf, uint32_t rgb){
  PackGRBsub(buf,    (rgb & 0x0000c000) >> 14);
  PackGRBsub(buf+3,  (rgb & 0x00003000) >> 12);
  PackGRBsub(buf+6,  (rgb & 0x00000c00) >> 10);
  PackGRBsub(buf+9,  (rgb & 0x00000300) >>  8);

  PackGRBsub(buf+12, (rgb & 0x00c00000) >> 22);
  PackGRBsub(buf+15, (rgb & 0x00300000) >> 20);
  PackGRBsub(buf+18, (rgb & 0x000c0000) >> 18);
  PackGRBsub(buf+21, (rgb & 0x00030000) >> 16);

  PackGRBsub(buf+24, (rgb & 0x000000c0) >> 6);
  PackGRBsub(buf+27, (rgb & 0x00000030) >> 4);
  PackGRBsub(buf+30, (rgb & 0x0000000c) >> 2);
  PackGRBsub(buf+33,  rgb & 0x00000003);
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

  spi_master.setFrequency(10000000);
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
    PackGRB(dma_tx_buf + 100 + i * 36, rgbmap[i]);
  }

  spi_master.transfer(dma_tx_buf, dma_rx_buf, BUFFER_SIZE);

  count++;
  prev_ms = curr_ms;
  return;
}