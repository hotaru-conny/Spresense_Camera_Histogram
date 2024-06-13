#include <Camera.h>
#include <Adafruit_ILI9341.h>

//ディスプレイの設定
#define TFT_DC  9
#define TFT_CS  10
Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC);
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

//画像の一辺の長さ
#define IMG_EDGE_LEN 128

//撮影データを入れる配列
uint8_t R[IMG_EDGE_LEN][IMG_EDGE_LEN];
uint8_t G[IMG_EDGE_LEN][IMG_EDGE_LEN];
uint8_t B[IMG_EDGE_LEN][IMG_EDGE_LEN];


//ヒストグラム描画時の高さ上限
#define HIST_MAX_HEIGHT (DISPLAY_HEIGHT / 3)
//ヒストグラムの棒がいくつあるか
#define HIST_NUM (HIST_MAX_HEIGHT)
//ヒストグラムデータ格納用
int histR[HIST_NUM];
int histG[HIST_NUM];
int histB[HIST_NUM];
int histMaxCommon = 0;

bool camInit(){
  CamErr err;
  err = theCamera.begin(1,CAM_VIDEO_FPS_30,IMG_EDGE_LEN,IMG_EDGE_LEN,CAM_IMAGE_PIX_FMT_YUV422,7);
  if (err != CAM_ERR_SUCCESS)
    {
      return false;
    }
  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_DAYLIGHT);
  if (err != CAM_ERR_SUCCESS)
    {
      return false;
    }
  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(1024,1024,CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS)
    {
      return false;
    }
  return true;
}

// RGB565を出力
bool camCapRGB128(uint8_t R[IMG_EDGE_LEN][IMG_EDGE_LEN], uint8_t G[IMG_EDGE_LEN][IMG_EDGE_LEN], uint8_t B[IMG_EDGE_LEN][IMG_EDGE_LEN], CamImage img){
  CamErr err = img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  uint16_t* imgbuf = (uint16_t*)img.getImgBuff();
  for (int n = 0; n < IMG_EDGE_LEN*IMG_EDGE_LEN; ++n) {
    R[(int)(n/IMG_EDGE_LEN)][n%IMG_EDGE_LEN] = (uint8_t)((imgbuf[n] & 0xf800) >> 11);
    G[(int)(n/IMG_EDGE_LEN)][n%IMG_EDGE_LEN] = (uint8_t)((imgbuf[n] & 0x07e0) >> 5 );
    B[(int)(n/IMG_EDGE_LEN)][n%IMG_EDGE_LEN] = (uint8_t)(imgbuf[n] & 0x001f);
  }
  return true;            
}

void SetTextOnDisplay(String str, int startX, int startY, int color){
  int len = str.length();
  //display.fillRect(0,224, 320, 240, ILI9341_BLACK);
  display.setTextSize(2);
  display.setCursor(startX, startY);
  display.setTextColor(color);
  display.println(str);
}

void calcHistogram(uint8_t data[IMG_EDGE_LEN][IMG_EDGE_LEN], int hist[HIST_NUM], int max_pixel_value){
  //ヒストグラム初期化
  for(int i=0; i<HIST_NUM; i++){
    hist[i] = 0;
  }
  //ヒストグラム作成
  for(int i=0; i<IMG_EDGE_LEN; i++){
    for(int j=0; j<IMG_EDGE_LEN; j++){
      //data[i][j]を0-max_pixel_valueの範囲から0-HIST_NUMの範囲にマッピング
      int index = (int)((float)data[i][j] / (float)max_pixel_value * (float)HIST_NUM) - 1;
      hist[index] += 1;
    }
  }
  //ヒストグラムの最大値取得
  int histMax = 0;
  for(int i=0; i<HIST_NUM; i++){
    if(hist[i] > histMax) histMax = hist[i];
  }
  //グローバル変数のヒストグラム最大値を更新
  if(histMax > histMaxCommon) histMaxCommon = histMax;
}
void drawHistogram(int hist[HIST_NUM], int startX, int startY, int color){
  //ヒストグラムの描画
  display.fillRect(startX, startY, HIST_NUM, HIST_MAX_HEIGHT, ILI9341_BLACK);
  int normHist = 0;
  for(int i=0; i<HIST_NUM; i++){
    //histMaxCommonが最大となるよう正規化
    if(histMaxCommon != 0){
      normHist = (int)((float)hist[i] * (float)HIST_MAX_HEIGHT / (float)histMaxCommon); 
    }
    //左右反転に注意
    display.drawLine(startX+HIST_NUM-i-1, startY+HIST_MAX_HEIGHT, startX+HIST_NUM-i-1, startY+HIST_MAX_HEIGHT-normHist, color);
  }
}

void CamCB(CamImage img) {
  camCapRGB128(R,G,B,img);
  Serial.println();

  Serial.println();
  img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  display.drawRGBBitmap(0, 0, (uint16_t *)img.getImgBuff(), IMG_EDGE_LEN, IMG_EDGE_LEN);

  //ヒストグラム描画開始位置
  int startX = IMG_EDGE_LEN + 5;
  int startY = 0;
  int textOffset = 5;
  histMaxCommon = 0;
  calcHistogram(R, histR, 31);
  calcHistogram(G, histG, 63);
  calcHistogram(B, histB, 31);

  SetTextOnDisplay("R", startX+HIST_NUM+textOffset, startY, ILI9341_RED);
  drawHistogram(histR, startX, startY, ILI9341_RED);
  SetTextOnDisplay("G", startX+HIST_NUM+textOffset, startY+HIST_MAX_HEIGHT, ILI9341_GREEN);
  drawHistogram(histG, startX, startY+HIST_MAX_HEIGHT, ILI9341_GREEN);
  SetTextOnDisplay("B", startX+HIST_NUM+textOffset, startY+2*HIST_MAX_HEIGHT, ILI9341_BLUE);
  drawHistogram(histB, startX, startY+HIST_MAX_HEIGHT*2, ILI9341_BLUE);
}

void setup() {
  //シリアル通信開始
  Serial.begin(115200);
  //ディスプレイ初期化
  display.begin();
  display.setRotation(3);
  display.fillScreen(ILI9341_BLACK);

  //カメラ初期化
  if(!camInit()){
    Serial.println("Failed to initialize camera");
  }

  //カメラストリーミング開始
  if (theCamera.startStreaming(true, CamCB) != CAM_ERR_SUCCESS){
    Serial.println("Failed to stream camera");
  }
}


void loop() { }
